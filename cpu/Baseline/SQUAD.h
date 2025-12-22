#ifndef _SQUAD_H_
#define _SQUAD_H_

#include <bits/stdc++.h>


template<typename ID_TYPE, typename DATA_TYPE>
class ReservoirSampling {
public:
    ReservoirSampling() {}
    ReservoirSampling(int n): max_size(n) {}
    ~ReservoirSampling() {}
    void insert(ID_TYPE key, DATA_TYPE value, int timestamp) {
        if (++current_items <= max_size) {
            rs_vec.push_back({key, value, timestamp});
        }
        else {
            double r = static_cast<double>(std::rand()) / RAND_MAX;
            int index = current_items * r;
            if (index < max_size) {
                rs_vec[index] = {key, value, timestamp};
            }
        }
    }
    std::vector<DATA_TYPE> query(ID_TYPE key, int timestamp) {
        std::vector<DATA_TYPE> result = {};
        for (auto item : rs_vec) {
            ID_TYPE k = std::get<0>(item);
            DATA_TYPE v = std::get<1>(item);
            int ts = std::get<2>(item);
            if (key == k && ts < timestamp) {
                result.push_back(k);
            }
        }
        return result;
    }
private:
    int max_size;
    int current_items = 0;
    std::vector<std::tuple<ID_TYPE, DATA_TYPE, int>> rs_vec = {};
};

template<typename DATA_TYPE>
class KLL {
public:
    KLL() {}
    KLL(int _N, double _c, int _s, int memory): N(_N), c(_c), s(_s) {
        /*
        N: length of the data stream
        c: decreasing rate of capacity
        s: number of levels with fixed capacity
        memory: memory of KLL-Polymer
        */
        int capacity_sum = memory / sizeof(DATA_TYPE);
        l = capacity_sum / (s + c / (1 - c));
        H = log2(1.0 * N / l) + 2;
        assert(s < H);
        for (int i = 0; i < H - s; ++i) {
            int capacity = std::max((int)ceil(1.0 * l * pow(c, H - s - i)), 2);
            if (capacity <= 2) {
                reservoir_level = i + 1;
            }
            else {
                std::vector<DATA_TYPE> buffer;
                array.push_back(buffer);
                max_size.push_back(capacity);
            }
        }
        for (int i = H - s; i < H; ++i) {
            std::vector<DATA_TYPE> buffer;
            array.push_back(buffer);
            max_size.push_back(l);
        }
        assert(array.size() + reservoir_level == H);
    }

    void insert(DATA_TYPE value) {
        current_size++;
        // Step 1: in lower level, handling sampled values
        current_reservoir_samples++;
        double r = static_cast<double>(std::rand()) / RAND_MAX;
        if (r <= 1.0 / current_reservoir_samples) {
            reservoir_value = value;
        }

        // Step 2: checking whether current_reservoir_samples reaches maximum value
        if (current_reservoir_samples == (1 << reservoir_level)) {
            // output reservoir_value to higher level
            current_reservoir_samples = 0;
            array[0].push_back(reservoir_value);
        }

        // Step 3: handling compressing operation in higher level
        for (int i = 0; i < H - reservoir_level - 1; ++i) {
            while (array[i].size() >= max_size[i]) {
                // group-by and sort operation
                std::sort(array[i].begin(), array[i].begin() + max_size[i]);
                double r = static_cast<double>(std::rand()) / RAND_MAX;
                int random_bit = r < 0.5 ? 0 : 1;
                for (int j = random_bit; j < max_size[i]; j += 2) {
                    array[i + 1].push_back(array[i][j]);
                }
                array[i].erase(array[i].begin(), array[i].begin() + max_size[i]);
            }
        }
    }

    int query_rank(DATA_TYPE value) {
        int rank = 0;
        if (reservoir_value <= value) {
            rank += current_reservoir_samples;
        }
        for (int i = 0; i < H - reservoir_level; ++i) {
            for (auto item : array[i]) {
                rank += (item <= value) * (1 << (i + reservoir_level));
            }
        }
        return rank;
    }

    double query_quantile(DATA_TYPE value) {
        return 1.0 * query_rank(value) / current_size;
    }

    DATA_TYPE query_value(double w) {
        std::vector<std::pair<DATA_TYPE, int>> value_weight_vec;
        value_weight_vec.push_back({reservoir_value, current_reservoir_samples});
        for (int i = 0; i < H - reservoir_level; ++i) {
            for (auto item : array[i]) {
                value_weight_vec.push_back({item, (1 << (i + reservoir_level))});
            }
        }
        std::sort(value_weight_vec.begin(), value_weight_vec.end(), 
                  [](auto const& x, auto const& y) {
                    return x.first < y.first;
                  });
        double target = w * current_size;
        double current = 0;
        for (int i = 0; i < value_weight_vec.size(); ++i) {
            current += value_weight_vec[i].second;
            if (current >= target) {
                return value_weight_vec[i].first;
            }
        }
        return value_weight_vec.back().first;
    }
    int query_frequency() {
        return current_size;
    }

private:
    double c;
    int l;
    int H;
    int N;
    int s;
    std::vector<std::vector<DATA_TYPE>> array;
    std::vector<int> max_size;
    int reservoir_level = 0;
    DATA_TYPE reservoir_value;
    int current_reservoir_samples = 0;
    int current_size = 0;
};

template<typename DATA_TYPE>
struct SS_bucket {
    int frequency = 0;
    KLL<DATA_TYPE>* kll;
    int timestamp;
};


template<typename ID_TYPE, typename DATA_TYPE>
class SpaceSaving {
public:
    SpaceSaving() {}
    SpaceSaving(int memory, int ms, int _N, double _c, int _s): max_size(ms), N(_N), c(_c), s(_s) {
        kll_memory_byte = 1024 * memory / max_size;
        hash_map.clear();
        sorted_set.clear();
    }
    ~SpaceSaving() {}

    void insert(ID_TYPE key, DATA_TYPE value, int timestamp) {
        if (hash_map.find(key) != hash_map.end()) {
            int frequency_before = hash_map[key].frequency;
            sorted_set.erase({frequency_before, key});
            hash_map[key].frequency += 1;
            hash_map[key].kll->insert(value);
            sorted_set.insert({frequency_before + 1, key});
        }
        else if (hash_map.size() < max_size) {
            KLL<DATA_TYPE>* kll_new = new KLL<DATA_TYPE>(N, c, s, kll_memory_byte);
            kll_new->insert(value);
            hash_map[key] = {1, kll_new, timestamp};
            sorted_set.insert({1, key});
        }
        else {
            auto min_it = *sorted_set.begin();
            ID_TYPE min_key = min_it.second;
            int32_t min_frequency = min_it.first;
            auto it = hash_map.find(min_key);
            sorted_set.erase({min_frequency, min_key});
            hash_map.erase(it);
            KLL<DATA_TYPE>* kll_new = new KLL<DATA_TYPE>(N, c, s, kll_memory_byte);
            kll_new->insert(value);
            hash_map[key] = {min_frequency + 1, kll_new, timestamp};
            sorted_set.insert({min_frequency + 1, key});
        }
    }

    std::pair<int, KLL<DATA_TYPE>> query_KLL(ID_TYPE key) {
        if (hash_map.find(key) != hash_map.end()) {
            return {hash_map[key].timestamp, *(hash_map[key].kll)};
        }
        else {
            KLL<DATA_TYPE>* kll_new = new KLL<DATA_TYPE>(N, c, s, kll_memory_byte);
            return {__INT32_MAX__, *kll_new};
        }
    }

    std::pair<int, int> query_frequency(ID_TYPE key) {
        if (hash_map.find(key) != hash_map.end()) {
            return {hash_map[key].timestamp, hash_map[key].kll->query_frequency()};
        }
        else {
            return {__INT32_MAX__, 0};
        }
    }
private:
    int max_size;
    int kll_memory_byte;
    int N;
    double c;
    int s;
    std::unordered_map<ID_TYPE, SS_bucket<DATA_TYPE>> hash_map;
    std::multiset<std::pair<int32_t, ID_TYPE>> sorted_set;
};






template<typename ID_TYPE, typename DATA_TYPE>
class SQUAD {
public:
    SQUAD() {}
    SQUAD(int memory, int _N, double c, int s, double memory_ratio, int ss_ms): N(_N) {
        rs_size = 1024 * memory_ratio * memory / (sizeof(ID_TYPE) + sizeof(DATA_TYPE));
        int ss_memory = (1 - memory_ratio) * memory;
        rs = new ReservoirSampling<ID_TYPE, DATA_TYPE>(rs_size);
        ss = new SpaceSaving<ID_TYPE, DATA_TYPE>(ss_memory, ss_ms, N, c, s);
    }
    void insert(ID_TYPE key, DATA_TYPE value) {
        rs->insert(key, value, timestamp);
        ss->insert(key, value, timestamp);
        timestamp++;
    }
    int query_frequency(ID_TYPE key) {
        auto res = ss->query_frequency(key);
        int timestamp = res.first, frequency = res.second;
        auto vec = rs->query(key, timestamp);
        return frequency + vec.size() * N / rs_size;
    }
    int query_rank(ID_TYPE key, DATA_TYPE value) {
        auto res = ss->query_KLL(key);
        int timestamp = res.first;
        KLL<DATA_TYPE> kll = res.second;
        auto vec = rs->query(key, timestamp);
        for (auto v : vec) {
            for (int i = 0; i < N / rs_size; ++i) {
                kll.insert(v);
            }
        }
        return kll.query_rank(value);
    }
    double query_quantile(ID_TYPE key, DATA_TYPE value) {
        return 1.0 * query_rank(key, value) / query_frequency(key);
    }
    DATA_TYPE query_value(ID_TYPE key, double w) {
        auto res = ss->query_KLL(key);
        int timestamp = res.first;
        KLL<DATA_TYPE> kll = res.second;
        auto vec = rs->query(key, timestamp);
        for (auto v : vec) {
            for (int i = 0; i < N / rs_size; ++i) {
                kll.insert(v);
            }
        }
        return kll.query_value(w);
    }
private:
    int N;
    int rs_size;
    int timestamp = 0;
    ReservoirSampling<ID_TYPE, DATA_TYPE>* rs;
    SpaceSaving<ID_TYPE, DATA_TYPE>* ss;
};


#endif