#ifndef _KLLPOLYMER_H_
#define _KLLPOLYMER_H_

#include <bits/stdc++.h>

template<typename ID_TYPE, typename DATA_TYPE>
class KLL_Polymer {
public:
    KLL_Polymer() {}
    KLL_Polymer(int _N, double _c, int _s, int memory): N(_N), c(_c), s(_s) {
        /*
        N: length of the data stream
        c: decreasing rate of capacity
        s: number of levels with fixed capacity
        memory: memory of KLL-Polymer
        */
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

    void insert(ID_TYPE key, DATA_TYPE value) {
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


template<typename ID_TYPE, typename DATA_TYPE>
class KLL_Polymer_DC : protected KLL_Polymer<ID_TYPE, DATA_TYPE> {
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::c;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::l;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::H;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::N;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::s;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::array;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::max_size;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::reservoir_level;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::reservoir_key;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::reservoir_value;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::current_reservoir_samples;
public:
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::query_rank;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::query_value;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::query_frequency;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::query_quantile;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::calculate_memory;
    using KLL_Polymer<ID_TYPE, DATA_TYPE>::calculate_reduced_weight_sum;
    KLL_Polymer_DC(int _N, double _c, int _s, int memory) {
        /*
        N: length of the data stream
        c: decreasing rate of capacity
        s: number of levels with fixed capacity
        memory: memory of KLL-Polymer
        */
        N = _N;
        c = _c;
        s = _s;
        int capacity_sum = memory * 1024 / (sizeof(ID_TYPE) + sizeof(DATA_TYPE));
        l = capacity_sum / (s + c / (1 - c));
        H = log2(1.0 * N / l) + 1;
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

    ~KLL_Polymer_DC() {}

    void insert(ID_TYPE key, DATA_TYPE value) {
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
        // lower level, using randomized compaction
        for (int i = 0; i < H - reservoir_level - s; ++i) {
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

        // top level, using deterministic compaction
        for (int i = H - reservoir_level - s; i < H - reservoir_level - 1; ++i) {
            while (array[i].size() >= max_size[i]) {
                std::map<ID_TYPE, std::vector<DATA_TYPE>> kv_array;
                std::set<ID_TYPE> key_set;
                int promoted_items = 0;
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
                    double r = static_cast<double>(std::rand()) / RAND_MAX;
                    int random_bit = r < 0.5 ? 0 : 1;
                    for (int j = random_bit; j < vec.size() + random_bit - 1; j += 2) {
                        array[i + 1].push_back({_key, vec[j]});
                        promoted_items++;
                    }
                }
                array[i].erase(array[i].begin(), array[i].begin() + max_size[i]);
            }
        }
    }    
};







#endif