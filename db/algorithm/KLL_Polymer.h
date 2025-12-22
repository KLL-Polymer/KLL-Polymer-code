#ifndef _KLLPOLYMER_H_
#define _KLLPOLYMER_H_

#include <bits/stdc++.h>
#include "BOBHash.h"

template<typename ID_TYPE, typename DATA_TYPE>
class KLL_Polymer {
public:
    static constexpr uint32_t BF_SIZE_BYTES = 1 << 20; 
    static constexpr int BF_HASH_FUNCS = 7;
    std::vector<uint8_t> bf_bitmap;
    KLL_Polymer() {}
    KLL_Polymer(int _N, double _c, int _s, int memory): N(_N), c(_c), s(_s) {
        /*
        N: length of the data stream
        c: decreasing rate of capacity
        s: number of levels with fixed capacity
        memory: memory of KLL-Polymer
        */
        bf_bitmap.assign(BF_SIZE_BYTES, 0);
        int capacity_sum = memory * 1024 / (sizeof(ID_TYPE) + sizeof(DATA_TYPE));
        l = capacity_sum / (s + c / (1 - c));
        H = log2(1.0 * N / l) + 2;
        assert(s < H);
        for (int i = 0; i < H - s; ++i) {
            int capacity = std::max((int)ceil(1.0 * l * pow(c, H - s - i)), 2);
            if (capacity <= 2) {
                reservoir_level = i + 1;
            }
            else {
                std::vector<std::pair<ID_TYPE, DATA_TYPE>> buffer;
                array.push_back(buffer);
                max_size.push_back(capacity);
            }
        }
        for (int i = H - s; i < H; ++i) {
            std::vector<std::pair<ID_TYPE, DATA_TYPE>> buffer;
            array.push_back(buffer);
            max_size.push_back(l);
        }
        assert(array.size() + reservoir_level == H);
    }

    ~KLL_Polymer() {}

    void bf_add(ID_TYPE key) {
        for (int i = 0; i < BF_HASH_FUNCS; ++i) {
            size_t bit_pos = hash(key, i) % (BF_SIZE_BYTES * 8);
            bf_bitmap[bit_pos / 8] |= (1 << (bit_pos % 8));
        }
    }

    static bool bf_check_raw(const std::string& b, ID_TYPE key) {
        if (b.size() < BF_SIZE_BYTES) return true; 
        const char* bf_ptr = b.data();

        for (int i = 0; i < BF_HASH_FUNCS; ++i) {
            size_t bit_pos = hash(key, i) % (BF_SIZE_BYTES * 8);
            uint8_t byte = (uint8_t)bf_ptr[bit_pos / 8];
            if (!(byte & (1 << (bit_pos % 8)))) return false;
        }
        return true; 
    }

    std::vector<std::pair<DATA_TYPE, int>> query_all(ID_TYPE key) {
        std::vector<std::pair<DATA_TYPE, int>> results;
        if (reservoir_key == key) {
            results.push_back({reservoir_value, current_reservoir_samples});
        }
        for (int i = 0; i < array.size(); ++i) {
            int weight = (1 << (i + reservoir_level)); // 该层样本代表的权重
            for (auto& item : array[i]) {
                if (item.first == key) {
                    results.push_back({item.second, weight});
                }
            }
        }
        return results;
    }

    std::string serialize() const {
        std::string b;
        b.append((char*)bf_bitmap.data(), BF_SIZE_BYTES);
        auto append = [&](const auto& v) { 
            b.append((char*)&v, sizeof(v)); 
        };
        append(c); append(l); append(H); append(N); append(s);
        append(reservoir_level); append(reservoir_key); append(reservoir_value); append(current_reservoir_samples);
        uint32_t sz = max_size.size(); append(sz);
        b.append((char*)max_size.data(), sz * sizeof(int));
        uint32_t out_sz = array.size(); append(out_sz);
        for (auto& v : array) {
            uint32_t in_sz = v.size(); append(in_sz);
            b.append((char*)v.data(), in_sz * sizeof(std::pair<ID_TYPE, DATA_TYPE>));
        }
        return b;
    }

    void deserialize(const std::string& b) {
        if (b.empty()) return;
        const char* p = b.data();
        bf_bitmap.assign((uint8_t*)p, (uint8_t*)p + BF_SIZE_BYTES);
        p += BF_SIZE_BYTES;
        auto read = [&](auto& v) { memcpy(&v, p, sizeof(v)); p += sizeof(v); };
        read(c); read(l); read(H); read(N); read(s);
        read(reservoir_level); read(reservoir_key); read(reservoir_value);
        read(current_reservoir_samples);
        uint32_t sz; read(sz); max_size.resize(sz);
        memcpy(max_size.data(), p, sz * sizeof(int)); p += sz * sizeof(int);
        uint32_t out_sz; read(out_sz); array.assign(out_sz, {});
        for (uint32_t i = 0; i < out_sz; ++i) {
            uint32_t in_sz; read(in_sz); array[i].resize(in_sz);
            memcpy(array[i].data(), p, in_sz * sizeof(std::pair<ID_TYPE, DATA_TYPE>));
            p += in_sz * sizeof(std::pair<ID_TYPE, DATA_TYPE>);
        }
    }


    void insert(ID_TYPE key, DATA_TYPE value) {
        // update bloom filter
        bf_add(key);

        // Step 1: in lower level, handling sampled values
        current_reservoir_samples++;
        double r = static_cast<double>(std::rand()) / RAND_MAX;
        if (r <= 1.0 / current_reservoir_samples) {
            reservoir_key = key;
            reservoir_value = value;
        }

        // Step 2: checking whether current_reservoir_samples reaches maximum value
        if (current_reservoir_samples == (1 << reservoir_level)) {
            // output reservoir_value to higher level
            current_reservoir_samples = 0;
            array[0].push_back({reservoir_key, reservoir_value});
        }

        // Step 3: handling compressing operation in higher level
        for (int i = 0; i < H - reservoir_level - 1; ++i) {
            while (array[i].size() >= max_size[i]) {
                // group-by and sort operation
                std::vector<std::pair<ID_TYPE, DATA_TYPE>> grouped_array;
                std::map<ID_TYPE, std::vector<DATA_TYPE>> kv_array;
                std::set<ID_TYPE> key_set;
                for (int j = 0; j < max_size[i]; ++j) {
                    if (key_set.find(array[i][j].first) == key_set.end()) {
                        key_set.insert(array[i][j].first);
                        kv_array[array[i][j].first] = {};
                    }
                    kv_array[array[i][j].first].push_back(array[i][j].second);
                }
                for (auto iv : kv_array) {
                    ID_TYPE _key = iv.first;
                    auto vec = iv.second;
                    std::sort(vec.begin(), vec.begin() + vec.size());
                    for (auto _value : vec) {
                        grouped_array.push_back({_key, _value});
                    }
                }
                double r = static_cast<double>(std::rand()) / RAND_MAX;
                int random_bit = r < 0.5 ? 0 : 1;
                for (int j = random_bit; j < max_size[i]; j += 2) {
                    array[i + 1].push_back(grouped_array[j]);
                }
                array[i].erase(array[i].begin(), array[i].begin() + max_size[i]);
            }
        }
    }

    int query_rank(ID_TYPE key, DATA_TYPE value) {
        int rank = 0;
        if (reservoir_key == key && reservoir_value <= value) {
            rank += current_reservoir_samples;
        }
        for (int i = 0; i < H - reservoir_level; ++i) {
            for (auto item : array[i]) {
                rank += ((item.first == key) && (item.second <= value)) * (1 << (i + reservoir_level));
            }
        }
        return rank;
    }

    int query_frequency(ID_TYPE key) {
        int frequency = 0;
        if (reservoir_key == key) {
            frequency += current_reservoir_samples;
        }
        for (int i = 0; i < H - reservoir_level; ++i) {
            for (auto item : array[i]) {
                frequency += (item.first == key) * (1 << (i + reservoir_level));
            }
        }
        return frequency;
    }

    double query_quantile(ID_TYPE key, DATA_TYPE value) {
        int rank = query_rank(key, value), frequency = query_frequency(key);
        return 1.0 * rank / frequency;
    }

    DATA_TYPE query_value(ID_TYPE key, double w) {
        int frequency = 0;
        std::vector<std::pair<DATA_TYPE, int>> value_weight_vec;
        if (reservoir_key == key) {
            value_weight_vec.push_back({reservoir_value, current_reservoir_samples});
            frequency += current_reservoir_samples;
        }
        for (int i = 0; i < H - reservoir_level; ++i) {
            for (auto item : array[i]) {
                if (item.first == key) {
                    value_weight_vec.push_back({item.second, (1 << (i + reservoir_level))});
                    frequency += (1 << (i + reservoir_level));
                }
            }
        }
        // assert(frequency == query_frequency(key));
        std::sort(value_weight_vec.begin(), value_weight_vec.end(), 
                  [](auto const& x, auto const& y) {
                    return x.first < y.first;
                  });

        double target = w * frequency;
        double current = 0;

        for (int i = 0; i < value_weight_vec.size(); ++i) {
            current += value_weight_vec[i].second;
            if (current >= target) {
                return value_weight_vec[i].first;
            }
        }
        return value_weight_vec.back().first;
    }

    double calculate_memory() {
        std::cout << "Height: " << H << "\n";
        std::cout << "Revervoir height: " << reservoir_level << "\n";
        std::cout << "Fixed height: " << s << "\n";
        double memory = 0;
        for (int i = 0; i < reservoir_level; ++i) {
            std::cout << 2 << " ";
        }
        for (int i = 0; i < H - reservoir_level; ++i) {
            memory += max_size[i] * (sizeof(ID_TYPE) + sizeof(DATA_TYPE));
            std::cout << max_size[i] << " ";
        }
        std::cout << "\n";
        return 1.0 * memory / 1024;
    }
    void calculate_reduced_weight_sum() {
        int sum_weight = current_reservoir_samples;
        for (int i = 0; i < H - reservoir_level; ++i) {
            sum_weight += array[i].size() * (1 << (reservoir_level + i));
        }
        std::cout << N - sum_weight << "\n";
    }
protected:
    double c;
    int l;
    int H;
    int N;
    int s;
    std::vector<std::vector<std::pair<ID_TYPE, DATA_TYPE>>> array;
    std::vector<int> max_size;
    int reservoir_level = 0;
    ID_TYPE reservoir_key;
    DATA_TYPE reservoir_value;
    int current_reservoir_samples = 0;
};



#endif