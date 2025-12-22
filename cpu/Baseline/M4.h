#ifndef _M4_H_
#define _M4_H_
#include <bits/stdc++.h>

enum FlowType {
    TINY = 1,
    MID = 2,
    HUGE = 4,
};

struct FlowItem {
    uint64_t key;
    uint64_t value;
};

bool rand_bit() {
    static std::random_device rd;
    static std::default_random_engine gen(rd());
    static std::bernoulli_distribution dis(0.5);
    return dis(gen);
}

struct rand_u32_generator {
    rand_u32_generator(uint32_t seed = 0, uint32_t max = UINT32_MAX)
        : gen(seed), dis(0, max) { }

    uint32_t operator()() {
        return dis(gen);
    }

private:
    std::default_random_engine gen;
    std::uniform_int_distribution<uint32_t> dis;
};


bool f64_equal(double a, double b) {
    return std::fabs(a - b) < 1e-5;
}

template<typename T>
void vec_insert_ordered(std::vector<T>& vec, const T& item) {
    auto it = vec.begin();
    for (; it != vec.end() && *it < item; ++it);
    vec.insert(it, item);
}

template<typename T, typename Comp>
void vec_insert_ordered(std::vector<T>& vec, const T& item,
                                Comp cmp) {
    auto it = vec.begin();
    for (; it != vec.end() && cmp(*it, item); ++it);
    vec.insert(it, item);
}  

template <typename T>
uint32_t vec_rank(const std::vector<T>& vec, const T& item,
                    bool inclusive) {
    auto stop = [&item, inclusive](const T& t) {
        return t > item || (!inclusive && t == item);
    };
    uint32_t i = 0;
    for (; i < vec.size() && !stop(vec[i]); ++i);
    return i;
}

template <typename T, typename Comp>
uint32_t vec_rank(const std::vector<T>& vec, const T& item,
                    bool inclusive, Comp cmp) {
    auto stop = [&item, inclusive, &cmp](const T& t) {
        return cmp(item, t) || (!inclusive && !cmp(t, item));
    };
    uint32_t i = 0;
    for (; i < vec.size() && !stop(vec[i]); ++i);
    return i;
}

template <typename T>
std::vector<T> vec_union(const std::vector<T>& v1, const std::vector<T>& v2) {
    std::vector<T> res;
    res.reserve(v1.size() + v2.size());
    std::set_union(v1.begin(), v1.end(), v2.begin(), v2.end(),
                    std::back_inserter(res));
    return res;
}





template<typename DATA_TYPE>
class Histogram {
public:
    Histogram() = default;

    Histogram(const std::vector<DATA_TYPE>& split, const std::vector<uint32_t>& height) :
        m_splitPoints(split), m_heights(height) {}

    ~Histogram() = default;


    std::vector<DATA_TYPE> splitPoints() const { return m_splitPoints; }
    std::vector<uint32_t> heights() const { return m_heights; }


    friend Histogram operator&(const Histogram& a, const Histogram& b) {
        std::vector<DATA_TYPE> split_points = vec_union(a.m_splitPoints, b.m_splitPoints);
        return Histogram<DATA_TYPE>::andAligned(a.split(split_points), b.split(split_points));
    }

    friend Histogram operator|(const Histogram& a, const Histogram& b) {
        std::vector<DATA_TYPE> split_points = vec_union(a.m_splitPoints, b.m_splitPoints);
        return Histogram<DATA_TYPE>::orAligned(a.split(split_points), b.split(split_points));
    }

    DATA_TYPE value(double nom_rank) const {
        if (nom_rank < 0.0 || nom_rank > 1.0) {
            throw std::invalid_argument("normalized rank out of range");
        }

        uint32_t total_height = 0;
        for (uint32_t h : m_heights) {
            total_height += h;
        }
        if (total_height == 0) {
            return 0;
        }
        uint32_t rk = nom_rank * total_height;

        uint32_t l = 0, l_rk = 0;
        for (; l_rk == 0 || l_rk < rk; l_rk += m_heights[l++]);

        uint32_t r = l;
        uint32_t r_rk = l_rk;
        l_rk -= m_heights[--l];

        double p = static_cast<double>(rk - l_rk) / (r_rk - l_rk);
        DATA_TYPE interval = m_splitPoints[r] - m_splitPoints[l];
        return m_splitPoints[l] + p * interval;
    }

    uint32_t rank(DATA_TYPE value) const {
        uint32_t total_height = 0;
        for (uint32_t h : m_heights) {
            total_height += h;
        }
        if (value > m_splitPoints[m_splitPoints.size() - 1]) {
            return total_height;
        }
        uint32_t l = 0, l_rk = 0;
        for (; m_splitPoints[l + 1] < value; l_rk += m_heights[l++]);
        double p = static_cast<double>(value - m_splitPoints[l]) / (m_splitPoints[l + 1] - m_splitPoints[l]) * m_heights[l];
        return l_rk + p;
    }


    double quantile(DATA_TYPE value) const {
        uint32_t total_height = 0;
        for (uint32_t h : m_heights) {
            total_height += h;
        }
        return 1.0 * rank(value) / total_height;
    }

private:
    std::vector<DATA_TYPE> m_splitPoints;  
    std::vector<uint32_t> m_heights; 

    Histogram split(const std::vector<DATA_TYPE>& split_points) const {
        const auto& s = split_points;
        assert(s.size() > 0);

        Histogram<DATA_TYPE> res = {s, std::vector<uint32_t>(s.size() - 1, 0)};

        constexpr double eps = 1e-6;
        uint32_t split_idx = 0;
        for (; s[split_idx] + eps < m_splitPoints.front(); ++split_idx);

        uint32_t l = 0;
        while (l < m_splitPoints.size() - 1 && split_idx < res.m_heights.size()) {
            double p = s[split_idx + 1] - s[split_idx];
            p /= m_splitPoints[l + 1] - m_splitPoints[l];

            assert(split_idx < res.m_heights.size());
            res.m_heights[split_idx] = std::rint(m_heights[l] * p);
            if (res.m_heights[split_idx] == 0 && m_heights[l] > 0) {
                res.m_heights[split_idx] = 1;
            }

            ++split_idx;
            if (m_splitPoints[l + 1] <= s[split_idx] + eps) {
                ++l;
            }
        }

        return res;
    }

    static Histogram andAligned(const Histogram& h1,
                                        const Histogram& h2) {
        Histogram<DATA_TYPE> res = h1;
        for (uint32_t i = 0; i < res.m_heights.size(); ++i) {
            res.m_heights[i] = std::min(res.m_heights[i], h2.m_heights[i]);
        }
        return res;
    }
    static Histogram orAligned(const Histogram& h1,
                                        const Histogram& h2) {
        Histogram<DATA_TYPE> res = h1;
        for (uint32_t i = 0; i < res.m_heights.size(); ++i) {
            res.m_heights[i] += h2.m_heights[i];
        }
        return res;
    }
};





template<typename DATA_TYPE>
class SortedView {
public:
    struct witem {
        DATA_TYPE value;
        uint32_t weight;

        static bool value_less(const witem& a, const witem& b) {
            return a.value < b.value;
        }

        static bool weight_less(const witem& a, const witem& b) {
            return a.weight < b.weight;
        }
    };

    using witem_iter = typename std::vector<witem>::iterator;
    using witem_const_iter = typename std::vector<witem>::const_iterator;
    using vec_witem = std::vector<witem>;

public:
    SortedView(uint32_t num) : totalWeight(0) {
        view.reserve(num);
    }
    SortedView() = delete;

    void insert(typename std::vector<DATA_TYPE>::const_iterator first, typename std::vector<DATA_TYPE>::const_iterator last, uint32_t weight) {
        for (auto it = first; it != last; ++it) {
            vec_insert_ordered(view, {*it, weight}, witem::value_less);
        }
    }
    
    void insert(witem_const_iter first, witem_const_iter last) {
        for (auto it = first; it != last; ++it) {
            vec_insert_ordered(view, *it, witem::value_less);
        }
    }

    void insert(DATA_TYPE value, uint32_t weight) {
        vec_insert_ordered(view, {value, weight}, witem::value_less);
    }

    void convertToCumulative() {
        // Merge items with same value.
        for (uint32_t i = 0; i < view.size(); ++i) {
            while (i + 1 < view.size() && view[i].value == view[i + 1].value) {
                view[i].weight += view[i + 1].weight;
                view.erase(view.begin() + i + 1);
            }
        }

        // Convert to cumulative view.
        totalWeight = 0;
        for (auto& entry : view) {
            totalWeight += entry.weight;
            entry.weight = totalWeight;
        }
    }


    uint32_t rank(DATA_TYPE item, bool inclusive = true) {
        if (view.empty()) {
            throw std::runtime_error("rank on empty view");
        }

        uint32_t rk = vec_rank(view, {item, 0}, inclusive, witem::value_less);
        return rk == 0 ? 0 : view[rk - 1].weight;
    }

    double nomRank(DATA_TYPE item, bool inclusive = true) {
        double rk = rank(item, inclusive);
        return rk / totalWeight;   // normalize
    }

    DATA_TYPE quantile(double nom_rank, bool inclusive = true) {
        if (view.empty()) {
            throw std::runtime_error("get quantile on empty view");
        }

        double tmp = nom_rank * totalWeight;
        uint32_t weight = inclusive ? std::ceil(tmp) : tmp;

        uint32_t rk = vec_rank(view, {0, weight}, !inclusive, witem::weight_less);
        return rk == view.size() ? view.back().value : view[rk].value;
    }

    operator Histogram<DATA_TYPE>() {
        if (view.empty()) {
            throw std::runtime_error("convert an empty view to histogram");
        }

        if (view.size() == 1) {
            std::vector<DATA_TYPE> split = {view[0].value - 1, view[0].value + 1};
            std::vector<uint32_t> height = {view[0].weight};
            return Histogram<DATA_TYPE>(split, height);
        }

        vec_witem vec = view;
        for (uint32_t i = 1; i < vec.size(); ++i) {
            vec[i].weight -= vec[i - 1].weight;
        }

        std::vector<DATA_TYPE> split(vec.size(), 0);
        std::vector<uint32_t> height(vec.size() - 1, 0);
        
        for (uint32_t i = 0; i < vec.size(); ++i) {
            split[i] = vec[i].value;
        }

        for (uint32_t i = 0; i + 1 < vec.size(); ++i) {
            height[i] += (vec[i].weight + vec[i + 1].weight + 1) / 2;
        }
        height.front() += (vec.front().weight) / 2;
        height.back() += (vec.back().weight + 1) / 2;

        return Histogram<DATA_TYPE>(split, height);
    }

private:
    std::vector<witem> view; 
    uint32_t totalWeight;
};




template<typename DATA_TYPE>
class mReqCmtor {
public:
    mReqCmtor(uint32_t lg_w_, uint32_t cap_): lg_w(lg_w_), cap(cap_) {
        items.reserve(cap);
    }

    mReqCmtor() = delete;

    bool full() {
        return size() >= cap;
    }

    uint32_t size() {
        return items.size();
    }

    uint32_t capacity() {
        return cap;
    }

    uint32_t lgWeight() {
        return lg_w;
    }

    uint32_t weight() {
        return 1U << lgWeight();
    }

    uint32_t memory() {
        return sizeof(DATA_TYPE) * capacity();
    }

    typename std::vector<DATA_TYPE>::iterator begin() {
        return items.begin();
    }

    typename std::vector<DATA_TYPE>::iterator end() {
        return items.end();
    }

    void insert(DATA_TYPE item) {
        if (full()) {
            throw std::logic_error("append to a full compactor");
        }
        vec_insert_ordered(items, item);
    }

    void compact(mReqCmtor& next) {
        if (!full()) {
            throw std::logic_error("compact a non-full compactor");
        }

        // output coin, coin+2, coin+4, ... into next compactor
        bool coin = rand_bit();
        for (uint32_t i = coin; i < size(); i += 2) {
            vec_insert_ordered(next.items, items[i]);
        }

        // clear this compactor
        items.clear();
    }

    uint32_t rank(DATA_TYPE item, bool inclusive = true) {
        return vec_rank(items, item, inclusive);
    }

    uint32_t weightedRank(uint32_t item, bool inclusive = true) {
        return rank(item, inclusive) * weight();
    }


private:
    uint32_t     lg_w;
    uint32_t     cap;
    std::vector<DATA_TYPE> items;
};




template<typename DATA_TYPE>
class mReqSketch {
    using vec_cmtor = std::vector<mReqCmtor<DATA_TYPE>>;
public:
    mReqSketch(uint32_t sketch_cap_, uint32_t cmtor_cap_) : itemNum(0), sketchCap(sketch_cap_) {
        // to satisfy the requirement that
        // cmtor_cap_ * (1 + 2 + ... + 2 ^ (cmtor_num - 1)) >= sketch_cap
        uint32_t cmtor_num = std::ceil(
            std::log2(static_cast<double>(sketchCap) / cmtor_cap_ + 1));

        cmtors.reserve(cmtor_num);
        for (uint32_t i = 0; i < cmtor_num; ++i) {
            cmtors.emplace_back(i, cmtor_cap_);
        }
    }

    ~mReqSketch() {}

    mReqSketch() = default;
    
    uint32_t size() {
        return itemNum;
    }
    bool empty() {
        return size() == 0;
    }

    bool full() {
        return size() >= sketchCap;
    }

    uint32_t memory() {
        return cmtors.capacity() * cmtors.front().memory();
    }

    void insert(DATA_TYPE item) {
        if (full()) {
            throw std::logic_error("append to a full mreq sketch");
        }

        // append to the first compactor
        ++itemNum;
        minItem = std::min(minItem, item);
        maxItem = std::max(maxItem, item);
        cmtors.front().insert(item);

        // compact if needed
        for (uint32_t i = 0; cmtors[i].full(); ++i) {
            assert (i + 1 < cmtors.size());
            cmtors[i].compact(cmtors[i + 1]);
        }
    }

    uint32_t rank(DATA_TYPE item, bool inclusive) {
        if (empty()) {
            throw std::runtime_error("rank on empty mreq sketch");
        }

        uint32_t rank = 0;
        // sum up ranks of all compactors
        for (auto& cmtor : cmtors) {
            rank += cmtor.weightedRank(item, inclusive);
        }
        return rank;
    }
    
    double nomRank(DATA_TYPE item, bool inclusive) {
        if (empty()) {
            throw std::runtime_error("rank on empty mreq sketch");
        }
        double rk = rank(item, inclusive);
        return rk / size(); // normalize
    }

    uint32_t quantile(double nom_rank, bool inclusive = true) {
        if (empty()) {
            throw std::runtime_error("get quantile on empty mreq sketch");
        }
        if (nom_rank < 0.0 || nom_rank > 1.0) {
            std::cerr << "normalized rank: " << nom_rank << std::endl;
            throw std::invalid_argument("normalized rank out of range");
        }

        auto view = setupSortedView();
        return view.quantile(nom_rank, inclusive);
    }

    operator Histogram<DATA_TYPE>() {
        if (empty()) {
            throw std::runtime_error("convert an empty mreq sketch to histogram");
        }

        auto view = setupSortedView();
        return static_cast<Histogram<DATA_TYPE>>(view);
    }


private:
    uint32_t itemNum; 
    uint32_t sketchCap; 
    vec_cmtor cmtors; 
    DATA_TYPE minItem = std::numeric_limits<DATA_TYPE>::max();  
    DATA_TYPE maxItem = std::numeric_limits<DATA_TYPE>::lowest(); 
    
    SortedView<DATA_TYPE> setupSortedView() {
        auto view = SortedView<DATA_TYPE>(size());

        for (auto& cmtor : cmtors) {
            view.insert(cmtor.begin(), cmtor.end(), cmtor.weight());
        }
        view.insert(minItem, 0);
        view.insert(maxItem, 0);

        view.convertToCumulative();

        return view;
    }
};

template<typename DATA_TYPE>
class TinyCnter {
public:
    TinyCnter(): cnt0(0), cnt1(0), cnt2(0), cnt3(0), maxItem(0) {}

    bool full(uint32_t idx) {
        return count(idx) == MAX_CNT;
    }

    bool empty(uint32_t idx) {
        return count(idx) == 0;
    }

    void insert(DATA_TYPE item, uint32_t idx) {
        checkIdx(idx);
        if (full(idx)) {
            throw std::logic_error("append to a full tiny counter");
        }
        switch (idx) {
            case 0: ++cnt0; break;
            case 1: ++cnt1; break;
            case 2: ++cnt2; break;
            case 3: ++cnt3; break;
        }
        maxItem = std::max(maxItem, item);
    }

    uint32_t count(uint32_t idx) {
        checkIdx(idx);
        switch (idx) {
            case 0: return cnt0;
            case 1: return cnt1;
            case 2: return cnt2;
            case 3: return cnt3;
        }

        throw std::runtime_error("shouldn't reach here");
    }

    uint32_t value() {
        return maxItem;
    }

    uint32_t memory() {
        return 5;
    }

private:
    uint8_t cnt0: 2;
    uint8_t cnt1: 2; 
    uint8_t cnt2: 2;
    uint8_t cnt3: 2;
    DATA_TYPE maxItem; 
    static constexpr uint32_t MAX_CNT = 3; 

    void checkIdx(uint32_t idx) {
        if (idx > MAX_CNT) {
            throw std::invalid_argument("tiny counter index out of range");
        }
    }
};


template <typename ID_TYPE, typename DATA_TYPE> 
class M4 {
    using vec_tiny = std::vector<TinyCnter<ID_TYPE>>;
    using vec_meta = std::vector<mReqSketch<DATA_TYPE>>;

public:
    M4(uint64_t mem_limit, uint32_t _hash_num = 3, uint32_t seed = 0) {
        hash_num = _hash_num;
        TinyCnter<ID_TYPE> tmp_lv0;
        mReqSketch<DATA_TYPE> tmp[4];
        for (uint32_t i = 1; i < 4; ++i) {
            tmp[i] = mReqSketch<DATA_TYPE>(cap[i], cmtor_cap[i]);
        }

        uint32_t bucket_num[LEVELS];
        bucket_num[0] = 1024 * mem_limit * mem_div[0] / tmp_lv0.memory();
        for (uint32_t i = 1; i < 4; ++i) {
            bucket_num[i] = 1024 * mem_limit * mem_div[i] / tmp[i].memory();
        }

        // allocate memory
        lv0.reserve(bucket_num[0]);
        lv1.reserve(bucket_num[1]);
        lv2.reserve(bucket_num[2]);
        lv3.reserve(bucket_num[3]);
        while (bucket_num[0]--) { lv0.emplace_back(); }
        while (bucket_num[1]--) { lv1.push_back(tmp[1]); }
        while (bucket_num[2]--) { lv2.push_back(tmp[2]); }
        while (bucket_num[3]--) { lv3.push_back(tmp[3]); }

        // initialize hash
        for (uint32_t i = 0; i < LEVELS; ++i) {
            hashVal[i].reserve(hash_num);
            for (uint32_t j = 0; j < hash_num; ++j) {
                hashVal[i].emplace_back(0);
            }
        }
    }

    uint32_t memory()  {
        uint32_t mem = 0;
        mem += lv0.size() * lv0.front().memory();
        mem += lv1.size() * lv1.front().memory();
        mem += lv2.size() * lv2.front().memory();
        mem += lv3.size() * lv3.front().memory();
        return mem;
    }

    uint32_t calcInsertLevel(ID_TYPE key)  {
        calcHash(key);
        for (uint32_t i = 0; i < LEVELS; ++i) {
            if (!hasAnyFull(i, key) || hasAnyEmpty(i, key)) {
                return i;
            }
        }

        throw::std::runtime_error("the whole META DiffSketch is full");
    }

    uint32_t calcQueryLevel(ID_TYPE key)  {
        calcHash(key);
        for (uint32_t i = 0; i < LEVELS; ++i) {
            if (i != 0 && hasAnyEmpty(i, key)) {
                return i - 1;
            }
            if (!hasAnyFull(i, key)) {
                return i;
            }
        }
        throw::std::runtime_error("the whole META DiffSketch is full");
    }

    void insertTiny(ID_TYPE key, DATA_TYPE value) {
        auto& hv = hashVal[0];
        for (uint32_t i = 0; i < hv.size(); ++i) {
            uint32_t pos = hv[i] / 4;
            uint32_t idx = hv[i] % 4;
            if (!lv0[pos].full(idx)) {
                lv0[pos].insert(value, idx);
            }
        }
    }

    void insertMETA(uint32_t level, ID_TYPE key, DATA_TYPE value) {
        auto& vec = getVecMETA(level);
        auto& hv = hashVal[level];
        for (uint32_t i = 0; i < hv.size(); ++i) {
            if (!vec[hv[i]].full()) {
                vec[hv[i]].insert(value);
            }
        }
    }

    Histogram<DATA_TYPE> doOR(ID_TYPE key) {
        uint32_t level = calcQueryLevel(key);

        if (level == 0) {
            throw std::runtime_error("combine() is not supported in level 0");
        }

        Histogram<DATA_TYPE> hist = doAND(level, key);
        for (uint32_t i = level - 1; i >= 1; --i) {
            if (hasAnyEmpty(i, key)) {
                continue;
            }
            hist = hist | doAND(i, key);
        }

        return hist;
    }

    Histogram<DATA_TYPE>doAND(uint32_t level, ID_TYPE key) {
        auto& vec = getVecMETA(level);
        auto& hv = hashVal[level];
        
        Histogram<DATA_TYPE> hist = static_cast<Histogram<DATA_TYPE>>(vec[hv[0]]);
        for (uint32_t i = 1; i < hv.size(); ++i) {
            hist = hist & vec[hv[i]];
        }
        
        return hist;
    }

    void calcHash(ID_TYPE key) {
        // This function lies in hot path.
        // So we endure the verbose code to improve performance.
        uint32_t mod;

        mod = 4 * lv0.size();
        for (uint32_t i = 0; i < hash_num; ++i) {
            hashVal[0][i] = hash_two_keys(key, 0, i) % mod;
        }
        mod = lv1.size();
        for (uint32_t i = 0; i < hash_num; ++i) {
            hashVal[1][i] = hash_two_keys(key, 1, i) % mod;
        }
        mod = lv2.size();
        for (uint32_t i = 0; i < hash_num; ++i) {
            hashVal[2][i] = hash_two_keys(key, 2, i) % mod;
        }
        mod = lv3.size();
        for (uint32_t i = 0; i < hash_num; ++i) {
            hashVal[3][i] = hash_two_keys(key, 3, i) % mod;
        }
    }

    bool isAllFull(uint32_t level, ID_TYPE key) {
        if (level == 0) {
            for (uint32_t i = 0; i < hashVal[0].size(); ++i) {
                uint32_t pos = hashVal[0][i] / 4;
                uint32_t idx = hashVal[0][i] % 4;
                if (!lv0[pos].full(key)) {
                    return false;
                }
            }
            return true;
        }

        auto& vec = getVecMETA(level);
        for (uint32_t i = 0; i < hashVal[level].size(); ++i) {
            if (!vec[hashVal[level][i]].full()) {
                return false;
            }
        }
        return true;
    }

    bool hasAnyFull(uint32_t level, ID_TYPE key) {
        if (level == 0) {
            for (uint32_t i = 0; i < hashVal[0].size(); ++i) {
                uint32_t pos = hashVal[0][i] / 4;
                uint32_t idx = hashVal[0][i] % 4;
                if (lv0[pos].full(idx)) {
                    return true;
                }
            }
            return false;
        }

        auto& vec = getVecMETA(level);
        for (uint32_t i = 0; i < hashVal[level].size(); ++i) {
            if (vec[hashVal[level][i]].full()) {
                return true;
            }
        }
        return false;
    }

    bool hasAnyEmpty(uint32_t level, ID_TYPE key) {
        if (level == 0) {
            for (uint32_t i = 0; i < hashVal[0].size(); ++i) {
                uint32_t pos = hashVal[0][i] / 4;
                uint32_t idx = hashVal[0][i] % 4;
                if (lv0[pos].empty(idx)) {
                    return true;
                }
            }
            return false;
        }

        auto& vec = getVecMETA(level);
        for (uint32_t i = 0; i < hashVal[level].size(); ++i) {
            if (vec[hashVal[level][i]].empty()) {
                return true;
            }
        }
        return false;
    }

    auto getVecMETA(uint32_t level) -> vec_meta& {
        switch (level) {
            case 1: return lv1;
            case 2: return lv2;
            case 3: return lv3;
        }

        throw std::runtime_error(
            "AndorSketch<META>::getVecMETA(): shouldn't reach here");
    }

    FlowType type(ID_TYPE key) {
        uint32_t level = calcQueryLevel(key);
        switch (level) {
            case 0: return TINY;
            case 1: return MID;
            case 2: return HUGE;
            case 3: return HUGE;
        }

        throw std::runtime_error(
            "AndorSketch<META>::getVecMETA(): shouldn't reach here");
    }

    void insert(ID_TYPE key, DATA_TYPE value) {
        uint32_t level = calcInsertLevel(key);
        if (level == 0) {
            insertTiny(key, value);
        } else {
            insertMETA(level, key, value);
        }
    }
    
    DATA_TYPE query_value(ID_TYPE key, double w) {
        return doOR(key).value(w);
    }

    int query_rank(ID_TYPE key, DATA_TYPE value) {
        return doOR(key).rank(value);
    }

    double query_quantile(ID_TYPE key, DATA_TYPE value) {
        return doOR(key).quantile(value);
    }



private:
    static constexpr uint32_t LEVELS = 4;      ///< Number of levels.
    static constexpr uint32_t cap[4] = {3, UINT8_MAX, UINT16_MAX, UINT32_MAX};
    static constexpr uint32_t cmtor_cap[4] = {0, 2, 2, 4};
    static constexpr double mem_div[4] = {0.03, 0.60, 0.35, 0.02};
    static constexpr uint32_t ddc_size[4] = {0, 20, 20, 35};
    int hash_num;
    
    vec_tiny lv0;
    vec_meta lv1;
    vec_meta lv2;
    vec_meta lv3;
    mutable std::vector<uint32_t> hashVal[LEVELS];
};



#endif