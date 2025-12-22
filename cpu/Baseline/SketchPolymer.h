#ifndef _SKETCHPOLYMER_H_
#define _SKETCHPOLYMER_H_

#include <bits/stdc++.h>
#include <BOBHash.h>

template<typename ID_TYPE>
class Stage1 {
public:
    Stage1() {}
    Stage1(uint32_t memory, uint32_t dim): d(dim) {
        m = memory * 1024 / sizeof(uint8_t) / d;
        counter = new uint8_t* [d];
        for (int i = 0; i < d; ++i) {
            counter[i] = new uint8_t [m];
            memset(counter[i], 0, m * sizeof(uint8_t));
        }
    }
    ~Stage1() {
        for (int i = 0; i < d; ++i) {
            delete[] counter[i];
        }
        delete[] counter;
    }
    void insert(ID_TYPE id) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id, i) % m;
            if(counter[i][index] < 255) counter[i][index]++;
        }
    }
    uint32_t query(ID_TYPE id) {
        uint32_t frequency = std::numeric_limits<uint32_t>::max();
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id, i) % m;
            frequency = std::min(frequency, (uint32_t)counter[i][index]);
        }
        return frequency;
    }
private:
    uint32_t d;
    uint32_t m;
    uint8_t** counter;
};

template<typename ID_TYPE>
class Stage2 {
public:
    Stage2() {}
    Stage2(uint32_t memory, uint32_t dim): d(dim) {
        m = memory * 1024 / (sizeof(uint32_t) + sizeof(uint32_t)) / d;
        frequency = new uint32_t* [d];
        value = new uint32_t* [d];
        for (int i = 0; i < d; ++i) {
            frequency[i] = new uint32_t [m];
            value[i] = new uint32_t [m];
            memset(frequency[i], 0, m * sizeof(uint32_t));
            memset(value[i], 0, m * sizeof(uint32_t));
        }
    }
    ~Stage2() {
        for (int i = 0; i < d; ++i) {
            delete[] frequency[i];
            delete[] value[i];
        }
        delete[] frequency;
        delete[] value;
    }
    void insert(ID_TYPE id, uint32_t t) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id, 33 + i) % m;
            frequency[i][index]++;
            value[i][index] = std::max(value[i][index], t);
        }
    }
    void query(ID_TYPE id, uint32_t* f, uint32_t* t) {
        *f = std::numeric_limits<uint32_t>::max();
        *t = std::numeric_limits<uint32_t>::max();
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id, 33 + i) % m;
            *f = std::min(*f, frequency[i][index]);
            *t = std::min(*t, value[i][index]);
        }
    }
private:
    uint32_t d;
    uint32_t m;
    uint32_t** frequency;
    uint32_t** value;
};

template<typename ID_TYPE>
class Stage3 {
public:
    Stage3() {}
    Stage3(uint32_t memory, uint32_t dim): d(dim) {
        m = memory * 1024 / sizeof(uint16_t) / d;
        counter = new uint16_t* [d];
        for (int i = 0; i < d; ++i) {
            counter[i] = new uint16_t [m];
            memset(counter[i], 0, m * sizeof(uint16_t));
        }
    }
    ~Stage3() {
        for (int i = 0; i < d; ++i) {
            delete[] counter[i];
        }
        delete[] counter;
    }
    void insert(ID_TYPE id, uint32_t t) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id * t, i) % m;
            if (counter[i][index] != std::numeric_limits<uint16_t>::max())
                counter[i][index]++;
        }
    }
    uint32_t query(ID_TYPE id, uint32_t t) {
        uint16_t frequency = std::numeric_limits<uint16_t>::max();
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id * t, i) % m;
            frequency = std::min(frequency, counter[i][index]);
        }
        return frequency;
    }
private:
    uint32_t d;
    uint32_t m;
    uint16_t** counter;
};


template<typename ID_TYPE>
class Stage4 {
public:
    Stage4() {}
    Stage4(uint32_t memory, uint32_t dim): d(dim) {
        m = memory * 1024 / d;
        counter = new bool* [d];
        for (int i = 0; i < d; ++i) {
            counter[i] = new bool [m];
            memset(counter[i], false, m * sizeof(bool));
        }
    }
    ~Stage4(){
        for (int i = 0; i < d; ++i) {
            delete[] counter[i];
        }
        delete[] counter;
    }
    void insert(ID_TYPE id, uint32_t t) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id - t, 23 + t) % m;
            counter[i][index] = true;
        }
    }
    bool query(ID_TYPE id, uint32_t t) {
        for (int i = 0; i < d; ++i) {
            uint32_t index = ::hash(id - t, 23 + t) % m;
            if (!counter[i][index])
                return false;
        }
        return true;
    }
private:
    uint32_t d;
    uint32_t m;
    bool** counter;
};

template<typename ID_TYPE, typename DATA_TYPE>
class SketchPolymer {
public:
    SketchPolymer() {}
    SketchPolymer(uint32_t memory, uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4, uint32_t t, double a,double s1,double s2,double s3,double s4):
    threshold(t), log_base(a) {
        stage1 = new Stage1<ID_TYPE>(s1 * memory, d1);
        stage2 = new Stage2<ID_TYPE>(s2 * memory, d2);
        stage3 = new Stage3<ID_TYPE>(s3 * memory, d3);
        stage4 = new Stage4<ID_TYPE>(s4 * memory, d4);
    }
    ~SketchPolymer() {
        delete stage1;
        delete stage2;
        delete stage3;
        delete stage4;
    }
    void insert(ID_TYPE key, DATA_TYPE T) {
        if (stage1->query(key) < threshold) {
            stage1->insert(key);
            return;
        }
        uint32_t t = (log(T) / log(log_base));
        stage2->insert(key, t);
        stage3->insert(key, t);
        stage4->insert(key, t);
    }
    DATA_TYPE query_value(ID_TYPE key, double w) {
        uint32_t f, t;
        stage2->query(key, &f, &t);
        int m = (1 - w) * f;
        while (m > 0 && t) {
            if (stage4->query(key, t)) {
                m -= stage3->query(key, t);
            }
            t--;
        }
        return pow(log_base, t + 0.5);
    }
    int query_frequency(ID_TYPE key) {
        uint32_t f, t;
        stage2->query(key, &f, &t);
        return (int)f; 
    }
    int query_rank(ID_TYPE key, DATA_TYPE value) {
        uint32_t f, t;
        stage2->query(key, &f, &t);
        uint32_t v = (log(value) / log(log_base));
        while (t > v && t) {
            if (stage4->query(key, t)) {
                f -= stage3->query(key, t);
            }
            t--;
        }
        return f;
    }
    int query_quantile(ID_TYPE key, DATA_TYPE value) {
        int rank = query_rank(key, value), frequency = query_frequency(key);
        return 1.0 * rank / frequency;
    }
private:
    Stage1<ID_TYPE>* stage1;
    Stage2<ID_TYPE>* stage2;
    Stage3<ID_TYPE>* stage3;
    Stage4<ID_TYPE>* stage4;
    uint32_t threshold;
    double log_base;
};



#endif