#ifndef _BENCHMARK_H_
#define _BENCHMARK_H_

#include <bits/stdc++.h>
#include <Mmap.h>
#include "CorrectDetector.h"
#include "SketchPolymer.h"
#include "KLL-Polymer.h"
#include "M4.h"
#include "HistSketch.h"
#include "SQUAD.h"


struct Tuple {
    uint64_t value;
    uint64_t key;
    bool operator<(const Tuple& other) const {
        return value < other.value; 
    }
};

struct Dataset_Tuple {
    char key[13];
    uint64_t value;
};


class Benchmark {
public:
	Benchmark(std::string PATH, std::string SAVE_PATH) {
        save_path = SAVE_PATH;
        // for caida 2018
        running_length = 20000000;
        theta = 1e-4;
        load_result = Load(PATH.c_str());
        auto dataset_caida = (Dataset_Tuple*)load_result.start;
        length = load_result.length / sizeof(Dataset_Tuple);
        std::cout << length << "\n";
        dataset = new Tuple [running_length];
        for (int i = 0; i <= running_length; ++i) {
            std::memcpy(&(dataset[i].key), dataset_caida[i].key, 8);
            dataset[i].value = dataset_caida[i].value;
        }

        // for mawi dataset
        // running_length = 10000000;
        // theta = 1e-3;
        // load_result = Load(PATH.c_str());
        // auto dataset_mawi = (Dataset_Tuple*)load_result.start;
        // length = load_result.length / sizeof(Dataset_Tuple);
        // std::cout << length << "\n";
        // dataset = new Tuple [running_length];
        // for (int i = 0; i < running_length; ++i) {
        //     std::memcpy(&(dataset[i].key), dataset_mawi[i].key, 8);
        //     dataset[i].value = dataset_mawi[i].value;
        // }

        // for synthetic dataset
        // running_length = 10000000;
        // theta = 1e-3;
        // load_result = Load(PATH.c_str());
        // dataset = (Tuple*)load_result.start;
        // length = load_result.length / sizeof(Tuple);
    }
	~Benchmark() {}
    void generate_ground_truth() {
        // obtaining ground truth  
        std::vector<uint64_t> value_vec;
        for (int i = 0; i < running_length; ++i) {
            if (id_map.find(dataset[i].key) == id_map.end()) {
                id_map[dataset[i].key] = 0;
            }
            id_map[dataset[i].key]++;
            value_vec.push_back(dataset[i].value);
        }
        std::cout << "Keys: " << id_map.size() << "\n";
        for (auto i : id_map) {
            if (i.second >= theta * running_length) {
                heavy_hitters.push_back(i.first);
            }
        }
        std::cout << "Heavy Hitters: " << heavy_hitters.size() << "\n";
        for (int i = 0; i < running_length; ++i) {
            correct_detector->insert(dataset[i].key, dataset[i].value);
        }
        sort(value_vec.begin(), value_vec.end());
        for (int i = 0; i < interval_num; ++i) {
            interval.push_back(value_vec[i * running_length / interval_num]);
        }
        interval.push_back(value_vec[value_vec.size() - 1] + 5);
        for (auto key : heavy_hitters) {
            query_hh_value[key] = {};
            for (double w = 0.01; w < 1; w += 0.01) {
                uint64_t value = correct_detector->query_value(key, w);
                int rank = correct_detector->query_rank(key, value);
                query_hh_value[key].push_back({value, rank});
            }
        }
    }
    void Run_KLL_Polymer(int memory) {
        // running KLL-Polymer
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "KLL-Polymer: \n";
            KLL_Polymer<uint64_t, uint64_t>* kll_polymer = new KLL_Polymer<uint64_t, uint64_t>(running_length, 2.0 / 3, 3, memory);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                kll_polymer->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = kll_polymer->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = kll_polymer->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";
            // kll_polymer->calculate_reduced_weight_sum();
            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }

    void Run_KLL_Polymer_DC(int memory) {
        // running KLL-Polymer
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "KLL-Polymer-DC: \n";
            KLL_Polymer_DC<uint64_t, uint64_t>* kll_polymer_dc = new KLL_Polymer_DC<uint64_t, uint64_t>(running_length, 2.0 / 3, 3, memory);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                kll_polymer_dc->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = kll_polymer_dc->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = kll_polymer_dc->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";
            // kll_polymer_dc->calculate_reduced_weight_sum();
            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }

 

    void Run_SketchPolymer(int memory) {
        // running SketchPolymer
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "SketchPolymer: \n";
            SketchPolymer<uint64_t, uint64_t>* sketch_polymer = new SketchPolymer<uint64_t, uint64_t>(memory, 3, 5, 5, 3, 50, 1.5, 0.05, 0.3, 0.5, 0.15);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                sketch_polymer->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = sketch_polymer->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = sketch_polymer->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";

            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }


    void Run_HistSketch(int memory) {
        // running HistSketch
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "HistSketch: \n";
            HistSketch<uint64_t, uint64_t>* hist_sketch = new HistSketch<uint64_t, uint64_t>(memory, interval);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                hist_sketch->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = hist_sketch->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = hist_sketch->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";

            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }


    void Run_M4(int memory) {
        // running M4
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "M4: \n";
            M4<uint64_t, uint64_t>* m4 = new M4<uint64_t, uint64_t>(memory, 3, 0);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                m4->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = m4->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = m4->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";

            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }


    void Run_SQUAD(int memory) {
        // running SQUAD
        for (int run_time = 0; run_time < 1; ++run_time) {
            // std::cout << "SQUAD: \n";
            SQUAD<uint64_t, uint64_t>* squad = new SQUAD<uint64_t, uint64_t>(memory, running_length, 2.0 / 3, 3, 0.1, 1 / theta);

            // insertion operation
            auto start_insert = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < running_length; ++i) {
                squad->insert(dataset[i].key, dataset[i].value);
            }
            auto end_insert = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_insert = end_insert - start_insert;
            double insert_throughput = running_length / elapsed_insert.count() / 1e6;

            // rank query, given value v, return its rank
            double are = 0.0, aqe = 0.0;
            std::map<uint64_t, std::vector<int>> result_map;
            for (auto key : heavy_hitters) {
                std::vector<int> vec;
                vec.resize(105);
                result_map[key] = vec;
            }

            auto start_query_rank = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    uint64_t value = query_hh_value[key][i - 1].first;
                    int predict_rank = squad->query_rank(key, value);
                    result_map[key][i - 1] = predict_rank;
                }
            }
            auto end_query_rank = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_rank = end_query_rank - start_query_rank;
            double query_rank_throughput = heavy_hitters.size() * 99 / elapsed_query_rank.count() / 1e6;

            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double error = fabs(result_map[key][i - 1] - query_hh_value[key][i - 1].second);
                    are += error;   
                }
            }
            are =  are / heavy_hitters.size() / 99;

            // quantile query, given quantile w, return value with w-quantile
            std::map<uint64_t, std::vector<uint64_t>> quantile_result_map;
            for (auto key : heavy_hitters) {
                std::vector<uint64_t> vec;
                vec.resize(105);
                quantile_result_map[key] = vec;
            }

            auto start_query_quantile = std::chrono::high_resolution_clock::now();
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    uint64_t predict_value = squad->query_value(key, w);
                    quantile_result_map[key][i - 1] = predict_value;
                }
            }
            auto end_query_quantile = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_query_quantile = end_query_quantile - start_query_quantile;
            double query_quantile_throughput = heavy_hitters.size() * 99 / elapsed_query_quantile.count() / 1e6;
            
            std::vector<double> error_vec = {};
            error_vec.resize(105);
            for (auto key : heavy_hitters) {
                for (int i = 1; i < 100; ++i) {
                    double w = i / 100.0;
                    double truth_quantile = correct_detector->query_quantile(key, quantile_result_map[key][i - 1]);
                    double error = fabs(truth_quantile - w);
                    aqe += error;
                    error_vec[i - 1] += error;
                }
            }
            aqe = aqe / heavy_hitters.size() / 99;
            std::cout << are << " " << aqe << " " << insert_throughput << " " << query_rank_throughput << " " << query_quantile_throughput << "\n";

            std::ofstream ofs(save_path, std::ios::out | std::ios::app);
            for (int i = 1; i < 100; ++i) {
                ofs << error_vec[i - 1] / heavy_hitters.size() << ",";
            }
            ofs << "\n";
        }
    }




private:
	std::string filename;
    std::string save_path;
    LoadResult load_result;
    Tuple *dataset;
    uint64_t length;
    uint64_t running_length;
    double theta;
    CorrectDetector<uint64_t, uint64_t>* correct_detector = new CorrectDetector<uint64_t, uint64_t>();
    std::map<uint64_t, uint32_t> id_map;
    std::vector<uint64_t> heavy_hitters;
    std::map<uint64_t, std::vector<std::pair<uint64_t, int>>> query_hh_value;
    std::vector<uint64_t> interval;
};

#endif
