#ifndef _CMS_H_
#define _CMS_H_

#include <bits/stdc++.h>
#include "BOBHash.h"

template<typename ID_TYPE>
class CMSketch {
public:
    CMSketch() {}
    CMSketch(uint32_t memory, uint32_t dim): d(dim) {
        w = memory * 1024 / sizeof(uint32_t) / d;
        counter = new uint32_t* [d];
        for (int i = 0; i < d; ++i) {
            counter[i] = new uint32_t [w];
            memset(counter[i], 0, w * sizeof(uint32_t));
        }
    }
    ~CMSketch() {
        for (int i = 0; i < d; ++i) {
            delete[] counter[i];
        }
        delete[] counter;
    }
    void insert(ID_TYPE key) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = hash(key, i + 50) % w;
            counter[i][index]++;
        }
    }
    uint32_t query(ID_TYPE key) {
        uint32_t frequency = std::numeric_limits<uint32_t>::max();
        for (int i = 0; i < d; ++i) {
            uint32_t index = hash(key, i + 50) % w;
            frequency = std::min(frequency, (uint32_t)counter[i][index]);
        }
        return frequency;
    }
private:
    uint32_t d;
    uint32_t w;
    uint32_t** counter;
};

#endif