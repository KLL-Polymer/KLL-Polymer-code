#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <map>
#include <unordered_set>
#include <cstring>
#include <fstream>
#include <numeric>
#include <algorithm>

#include "rocksdb/include/rocksdb/db.h"
#include "rocksdb/include/rocksdb/options.h"
#include "rocksdb/include/rocksdb/table.h"
#include "rocksdb/include/rocksdb/filter_policy.h"
#include "rocksdb/include/rocksdb/slice_transform.h"
#include "rocksdb/include/rocksdb/write_batch.h"

#include "algorithm/KLL_Collector.h"
#include "algorithm/KLL_Polymer.h"
#include "algorithm/CMSketch.h"

using namespace std;

typedef __uint128_t uint128; 
typedef uint64_t    ID_TYPE; 
typedef uint64_t    DATA_TYPE;

struct FileRecord { uint64_t val; uint64_t key; };

struct GlobalStat {
    map<DATA_TYPE, int> value_weights;
    void add_sample(DATA_TYPE val, int weight) { value_weights[val] += weight; }

    DATA_TYPE get_quantile(double q) {
        long long total_w = 0;
        for (auto const& [v, w] : value_weights) total_w += w;
        if (total_w == 0) return 0;
        long long target = q * total_w;
        long long current = 0;
        for (auto const& [v, w] : value_weights) {
            current += w;
            if (current >= target) return v;
        }
        return value_weights.rbegin()->first;
    }
};

double get_ground_truth(const std::vector<DATA_TYPE>& vec, DATA_TYPE t) {
    if (vec.empty()) return 0.0;
    size_t count = 0;
    for (const auto& val : vec) { if (val <= t) count++; }
    return static_cast<double>(count) / vec.size();
}


std::map<ID_TYPE, std::vector<DATA_TYPE>> RunIngestion(rocksdb::DB* db, CMSketch<uint64_t>* cms, const string& filename, int total_items) {
    ifstream fin(filename, ios::binary);
    if (!fin) { cerr << "Error opening " << filename << endl; exit(1); }

    rocksdb::WriteOptions wo; wo.disableWAL = true;
    uint64_t suffix_counter = 0;
    std::map<ID_TYPE, std::vector<DATA_TYPE>> key_map;
    rocksdb::WriteBatch batch;

    cout << "Ingesting " << total_items << " items..." << endl;
    for (int i = 0; i < total_items; ++i) {
        FileRecord rec;
        if (!fin.read((char*)&rec, sizeof(FileRecord))) break;

        key_map[rec.key].push_back(rec.val);
        cms->insert(rec.key);

        uint128 full_key = 0;
        memcpy((char*)&full_key, &rec.key, 8);
        memcpy((char*)&full_key + 8, &suffix_counter, 8);
        suffix_counter++;

        batch.Put(rocksdb::Slice((char*)&full_key, 16), rocksdb::Slice((char*)&rec.val, 8));
        if (i % 10000 == 0) { db->Write(wo, &batch); batch.Clear(); }
        if (i % 1000000 == 0 && i > 0) { db->Flush(rocksdb::FlushOptions()); cout << "  Loaded " << i / 1000000 << "M..." << endl; }
    }
    db->Write(wo, &batch);
    db->Flush(rocksdb::FlushOptions());
    return key_map;
}

void RunPerformanceTest(rocksdb::DB* db, CMSketch<uint64_t>* cms, const vector<ID_TYPE>& query_keys, 
                        const rocksdb::TablePropertiesCollection& props, const vector<rocksdb::LiveFileMetaData>& files) {
    vector<double> latencies;
    cout << "Running Performance Test (Latency)..." << endl;

    auto total_start = chrono::high_resolution_clock::now();
    for (ID_TYPE target_id : query_keys) {
        char prefix[8]; memcpy(prefix, &target_id, 8);
        rocksdb::Slice target_slice(prefix, 8);
        
        auto start = chrono::high_resolution_clock::now();
        GlobalStat stat;
        if (cms->query(target_id) >= 10000) { // Sketch Path
            for (const auto& file : files) {
                if (target_slice.compare(rocksdb::Slice(file.smallestkey.data(), 8)) >= 0 &&
                    target_slice.compare(rocksdb::Slice(file.largestkey.data(), 8)) <= 0) {
                    auto it_prop = props.find(file.db_path + file.name);
                    if (it_prop != props.end()) {
                        auto it_blob = it_prop->second->user_collected_properties.find("rocksdb.kll_polymer.data");
                        if (it_blob != it_prop->second->user_collected_properties.end()) {
                            if (KLL_Polymer<ID_TYPE, DATA_TYPE>::bf_check_raw(it_blob->second, target_id)) {
                                KLL_Polymer<ID_TYPE, DATA_TYPE> sst_sketch;
                                sst_sketch.deserialize(it_blob->second);
                                auto samples = sst_sketch.query_all(target_id);
                                for (auto& p : samples) stat.add_sample(p.first, p.second);
                            }
                        }
                    }
                }
            }
            volatile DATA_TYPE res = stat.get_quantile(0.5);
        } else { // Baseline Path
            rocksdb::ReadOptions ro; ro.prefix_same_as_start = true;
            auto it = db->NewIterator(ro);
            for (it->Seek(target_slice); it->Valid(); it->Next()) {
                uint64_t v; memcpy(&v, it->value().data(), 8);
                stat.add_sample(v, 1);
            }
            volatile DATA_TYPE res = stat.get_quantile(0.5);
            delete it;
        }
        auto end = chrono::high_resolution_clock::now();
        latencies.push_back(chrono::duration<double, milli>(end - start).count());
    }
    auto total_end = chrono::high_resolution_clock::now();
    double total_time_s = chrono::duration<double>(total_end - total_start).count();

    sort(latencies.begin(), latencies.end());

    for (int i = 70; i <= 99; ++i) {
        std::cout << latencies[static_cast<size_t>(0.01 * latencies.size() * i)] << endl;
    }
}

void RunErrorAnalysis(const vector<ID_TYPE>& query_keys, map<ID_TYPE, vector<DATA_TYPE>>& key_map,
                      rocksdb::DB* db, const rocksdb::TablePropertiesCollection& props, const vector<rocksdb::LiveFileMetaData>& files) {
    cout << "Running Error Analysis (0.01 - 0.99)..." << endl;
    vector<double> error_vec_per_quantile(99, 0.0);

    for (ID_TYPE target_id : query_keys) {
        char prefix[8]; memcpy(prefix, &target_id, 8);
        rocksdb::Slice target_slice(prefix, 8);
        GlobalStat sketch_stat;
        
        for (const auto& file : files) {
            if (target_slice.compare(rocksdb::Slice(file.smallestkey.data(), 8)) >= 0 &&
                target_slice.compare(rocksdb::Slice(file.largestkey.data(), 8)) <= 0) {
                auto it_prop = props.find(file.db_path + file.name);
                if (it_prop != props.end() && it_prop->second->user_collected_properties.count("rocksdb.kll_polymer.data")) {
                    const string& blob = it_prop->second->user_collected_properties.at("rocksdb.kll_polymer.data");
                    if (KLL_Polymer<ID_TYPE, DATA_TYPE>::bf_check_raw(blob, target_id)) {
                        KLL_Polymer<ID_TYPE, DATA_TYPE> sst_sketch;
                        sst_sketch.deserialize(blob);
                        auto samples = sst_sketch.query_all(target_id);
                        for (auto& p : samples) sketch_stat.add_sample(p.first, p.second);
                    }
                }
            }
        }

        for (int i = 1; i <= 99; ++i) {
            double q = i / 100.0;
            DATA_TYPE res = sketch_stat.get_quantile(q);
            error_vec_per_quantile[i - 1] += fabs(q - get_ground_truth(key_map[target_id], res));
        }
    }

    cout << "Quantile,Avg_Error" << endl;
    for (int i = 1; i <= 99; ++i) {
        cout << i/100.0 << "," << error_vec_per_quantile[i - 1] / query_keys.size() << endl;
    }
}

void RunBaselinePerformanceTest(rocksdb::DB* db, const vector<ID_TYPE>& query_keys) {
    vector<double> latencies;
    cout << "Phase 4: Running Pure Baseline Performance Test (Iterator Only)..." << endl;

    auto total_start = chrono::high_resolution_clock::now();
    for (ID_TYPE target_id : query_keys) {
        char prefix[8]; memcpy(prefix, &target_id, 8);
        rocksdb::Slice target_slice(prefix, 8);
        
        auto start = chrono::high_resolution_clock::now();
        
        rocksdb::ReadOptions ro; 
        ro.prefix_same_as_start = true;
        auto it = db->NewIterator(ro);
        GlobalStat stat;
        for (it->Seek(target_slice); it->Valid(); it->Next()) {
            uint64_t v; 
            memcpy(&v, it->value().data(), 8);
            stat.add_sample(v, 1);
        }
        volatile DATA_TYPE res = stat.get_quantile(0.5); 
        delete it;

        auto end = chrono::high_resolution_clock::now();
        latencies.push_back(chrono::duration<double, milli>(end - start).count());
    }
    auto total_end = chrono::high_resolution_clock::now();
    double total_time_s = chrono::duration<double>(total_end - total_start).count();

    sort(latencies.begin(), latencies.end());
    for (int i = 70; i <= 99; ++i) {
        std::cout << latencies[static_cast<size_t>(0.01 * latencies.size() * i)] << endl;
    }
}


int main(int argc, char* argv[]) {
    string db_path = "/tmp/rocksdb_kll_final_benchmark";
    string data_file = argv[1];
    string mode = argv[2];
    int kll_memory = atoi(argv[3]);
    std::cout << data_file << " " << mode << " " << kll_memory << "\n";

    rocksdb::Options options;
    options.create_if_missing = true;
    options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(8));
    rocksdb::BlockBasedTableOptions table_options;
    table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
    options.table_properties_collector_factories.push_back(
        make_shared<KLLCollectorFactory<uint128, ID_TYPE, DATA_TYPE>>(100000, 2.0 / 3, 2, kll_memory)
    );

    rocksdb::DB* db;
    rocksdb::DestroyDB(db_path, options);
    CMSketch<uint64_t> cms(1024, 3);
    rocksdb::DB::Open(options, db_path, &db);

    auto key_map = RunIngestion(db, &cms, data_file, 10000000);
    vector<ID_TYPE> query_keys;

    if (mode == "mixed") {
        for (auto const& [id, vec] : key_map) { 
            if (vec.size() >= 150) 
                query_keys.push_back(id); 
        }
    }
    else if (mode == "heavy") {
        for (auto const& [id, vec] : key_map) { 
            if (vec.size() >= 2000) 
                query_keys.push_back(id); 
        }
    }
    else {
        // shall not reach here
        assert(0);
    }

    vector<rocksdb::LiveFileMetaData> files;
    db->GetLiveFilesMetaData(&files);
    rocksdb::TablePropertiesCollection props;
    db->GetPropertiesOfAllTables(&props);

    RunPerformanceTest(db, &cms, query_keys, props, files);
    RunErrorAnalysis(query_keys, key_map, db, props, files);
    RunBaselinePerformanceTest(db, query_keys);
    delete db;
    return 0;
}