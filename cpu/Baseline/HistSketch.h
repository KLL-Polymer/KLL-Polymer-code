#ifndef _HISTSKETCH_H_
#define _HISTSKETCH_H_

#include <bits/stdc++.h>
#include <BOBHash.h>

const int interval_num = 16;
const double eps = 1e-8;


template<typename ID_TYPE>
struct Bucket {
    ID_TYPE key;
    uint32_t histogram[interval_num];
    uint32_t negative_vote;
    bool flag;
    int frequency() {
        int sum = 0;
        for (int i = 0; i < interval_num; ++i) {
            sum += histogram[i];
        }
        return sum;
    }
    void clear() {
        key = 0;
        negative_vote = 0;
        flag = false;
        memset(histogram, 0, interval_num * sizeof(uint16_t));
    }
};

template<typename ID_TYPE>
struct key_interval_tuple {
    ID_TYPE key;
    uint32_t interval_key;
    uint32_t frequency;
};



template<typename ID_TYPE>
class HashTable {
public:
    HashTable(uint32_t memory) {
        size = memory * 1024 / sizeof(Bucket<ID_TYPE>);
        bucket_array = new Bucket<ID_TYPE>[size];
        memset(bucket_array, 0, size * sizeof(Bucket<ID_TYPE>));
    }
    ~HashTable() {
        delete[] bucket_array;
    }
    std::vector<key_interval_tuple<ID_TYPE>> insert(ID_TYPE key, uint32_t interval_key) {
        uint32_t index = ::hash(key, 99) % size;
        if (bucket_array[index].key == key) {
            // key match
            bucket_array[index].histogram[interval_key]++;
            return {};
        }
        else if (!bucket_array[index].frequency()) {
            // empty bucket
            bucket_array[index].key = key;
            bucket_array[index].histogram[interval_key]++;
            return {};
        }
        else {
            // hash collision
            if (++bucket_array[index].negative_vote > bucket_array[index].frequency()) {
                std::vector<key_interval_tuple<ID_TYPE>> vec;
                for (uint32_t i = 0; i < interval_num; ++i) {
                    vec.push_back({bucket_array[index].key, i, bucket_array[index].histogram[i]});
                }
                bucket_array[index].clear();
                bucket_array[index].key = key;
                bucket_array[index].flag = true;
                bucket_array[index].histogram[interval_key]++;
                return vec;
            }
            else {
                return std::vector<key_interval_tuple<ID_TYPE>>{{key, interval_key, 1}};
            }
        }
    }

    std::vector<uint32_t> query(ID_TYPE key) {
        uint32_t index = ::hash(key, 99) % size;
        if (bucket_array[index].key == key) {
            std::vector<uint32_t> vec = {bucket_array[index].flag};
            for (int i = 0; i < interval_num; ++i) {
                vec.push_back(bucket_array[index].histogram[i]);
            }
            return vec;
        }
        else {
            std::vector<uint32_t> vec = {1};
            vec.insert(vec.end(), interval_num, 0);
            return vec;
        }
    }



private:
    int size;
    Bucket<ID_TYPE>* bucket_array;
};



template<typename ID_TYPE>
class CMSketch {
public:
	CMSketch(uint32_t memory) {
        d = 4;
        w = memory * 1024 / (sizeof(uint32_t)) / d;
        array = new uint32_t* [d];
		for (int i = 0; i < d; ++i) {
			array[i] = new uint32_t [w];
			memset(array[i], 0, w * sizeof(uint32_t));
		}
	}
	~CMSketch() {
		for (int i = 0; i < d; ++i) {
			delete[] array[i];
		}
		delete[] array;
	}

    void insert(ID_TYPE key, uint32_t interval_key, uint32_t frequency) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = hash_two_keys(key, interval_key, i) % w;
            array[i][index] += frequency;
        }
    }

    uint32_t query(ID_TYPE key, uint32_t interval_key) {
        uint32_t result = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < d; ++i) {
            uint32_t index = hash_two_keys(key, interval_key, i) % w;
            result = std::min(result, array[i][index]);
        }
        return result;
    }

private:
	int d;
	int w;
	uint32_t** array;
};


template<typename ID_TYPE, typename DATA_TYPE>
class HistSketch {
public:
    HistSketch(uint32_t memory, std::vector<DATA_TYPE> _interval) {
        hash_table = new HashTable<ID_TYPE>(0.5 * memory);
        cmsketch = new CMSketch<ID_TYPE>(0.5 * memory);
        interval = _interval;
    }

    void insert(ID_TYPE key, DATA_TYPE value) {
        auto it = std::upper_bound(interval.begin(), interval.end(), value);
        uint32_t interval_key = std::distance(interval.begin(), it) - 1;
        assert(interval_key >= 0 && interval_key < interval_num);
        auto vec = hash_table->insert(key, interval_key);
        for (auto item : vec) {
            cmsketch->insert(item.key, item.interval_key, item.frequency);
        }
    }

    uint32_t query_frequency(ID_TYPE key) {
        auto vec = query_key(key);
        uint32_t frequency = 0;
        for (int i = 0; i < interval_num; ++i) {
            frequency += vec[i];
        }
        return frequency;
    }

    std::vector<uint32_t> query_key(ID_TYPE key) {
        auto vec = hash_table->query(key);
        uint32_t flag = vec.front();
        vec.erase(vec.begin());
        if (flag) {
            for (int i = 0; i < interval_num; ++i) {
                vec[i] += cmsketch->query(key, i);
            }
        }
        return vec;
    }

    uint32_t query_rank(ID_TYPE key, DATA_TYPE value) {
        if (value < interval[0]) {
            return 0;
        }
        if (value >= interval[interval_num]) {
            // shall not reach here
            assert(0);
        }
        auto vec = query_key(key);
        int frequency = query_frequency(key);
        int sum = 0, interval_key = 0;
        for (; interval_key < interval_num; ++interval_key) {
            if (interval[interval_key + 1] <= value) {
                sum += vec[interval_key];
            }
            else {
                break;
            }
        }
        assert(value >= interval[interval_key] && value < interval[interval_key + 1]);
        double scale = 1.0 * (value - interval[interval_key]) / (interval[interval_key + 1] - interval[interval_key] + eps);
        sum += scale * vec[interval_key];
        return sum;
    }
    double query_quantile(ID_TYPE key, DATA_TYPE value) {
        return 1.0 * query_rank(key, value) / (query_frequency(key) + eps);
    }

    DATA_TYPE query_value(ID_TYPE key, double w) {
        auto vec = query_key(key);
        int frequency = query_frequency(key);
        int rank = w * frequency;
        

        int sum = 0, interval_key = 0;
        for (; interval_key < interval_num; ++interval_key) {
            if (sum + vec[interval_key] >= rank) {
                break;
            }
            else {
                sum += vec[interval_key];
            }
        }
        double scale = 1.0 * (rank - sum) / (vec[interval_key] + eps);
        assert (scale >= 0 && scale <= 1);
        DATA_TYPE value = interval[interval_key] + scale * (interval[interval_key + 1] - interval[interval_key]);
        assert(value >= interval[interval_key] && value <= interval[interval_key + 1]);
        return value;
    }

private:
    std::vector<DATA_TYPE> interval;
    HashTable<ID_TYPE>* hash_table;
    CMSketch<ID_TYPE>* cmsketch;
};





#endif
