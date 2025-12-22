#ifndef _CORRECT_H_
#define _CORRECT_H_

#include <bits/stdc++.h>

template<typename ID_TYPE, typename DATA_TYPE>
class CorrectDetector {
public:
	CorrectDetector() {
		record.clear();
		id_set.clear();
	}
	~CorrectDetector() {}
	void insert(ID_TYPE key, DATA_TYPE value) {
		if (id_set.find(key) == id_set.end()) {
			id_set.insert(key);
			record[key] = {};
		}
		record[key].push_back(value);
	}
	uint64_t query_value(ID_TYPE key, double w) {
		uint32_t index = record[key].size() * w;
		sort(record[key].begin(), record[key].end());
		return record[key][index - 1];
	}
	int32_t query_frequency(ID_TYPE key) {
		return record[key].size();
	}
	double query_quantile(ID_TYPE key, DATA_TYPE value) {
		sort(record[key].begin(), record[key].end());
		auto upper = std::upper_bound(record[key].begin(), record[key].end(), value);
		int count = std::distance(record[key].begin(), upper);
		return 1.0 * count / record[key].size();
	}
	int query_rank(ID_TYPE key, DATA_TYPE value) {
		sort(record[key].begin(), record[key].end());
		auto upper = std::upper_bound(record[key].begin(), record[key].end(), value);
		int count = std::distance(record[key].begin(), upper);
		return count;
	}

private:
	std::map<ID_TYPE, std::vector<DATA_TYPE>> record;
	std::set<ID_TYPE> id_set;
};



#endif