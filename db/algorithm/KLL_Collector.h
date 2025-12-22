#ifndef KLL_COLLECTOR_H
#define KLL_COLLECTOR_H

#include "rocksdb/table_properties.h"
#include "KLL_Polymer.h"
#include <cstring>


template<typename Record_TYPE, typename ID_TYPE, typename DATA_TYPE>
class KLLCollector : public rocksdb::TablePropertiesCollector {
public:
    KLLCollector(int n, double c, int s, int m) : sketch_(n, c, s, m) {}

    rocksdb::Status AddUserKey(const rocksdb::Slice& key, const rocksdb::Slice& value,
                               rocksdb::EntryType, rocksdb::SequenceNumber, uint64_t) override {

        if (key.size() >= sizeof(Record_TYPE) && value.size() >= sizeof(DATA_TYPE)) {
            ID_TYPE true_key;
            DATA_TYPE val;

            std::memcpy(&true_key, key.data(), sizeof(ID_TYPE));
            std::memcpy(&val, value.data(), sizeof(DATA_TYPE));

            sketch_.insert(true_key, val);
        }
        return rocksdb::Status::OK();
    }

    rocksdb::Status Finish(rocksdb::UserCollectedProperties* props) override {
        props->insert({"rocksdb.kll_polymer.data", sketch_.serialize()});
        return rocksdb::Status::OK();
    }

    rocksdb::UserCollectedProperties GetReadableProperties() const override {
        return rocksdb::UserCollectedProperties();
    }

    const char* Name() const override { return "KLLCollector"; }

private:
    KLL_Polymer<ID_TYPE, DATA_TYPE> sketch_;
};


template<typename Record_TYPE, typename ID_TYPE, typename DATA_TYPE>
class KLLCollectorFactory : public rocksdb::TablePropertiesCollectorFactory {
public:
    KLLCollectorFactory(int n, double c, int s, int m) : n_(n), c_(c), s_(s), m_(m) {}

    rocksdb::TablePropertiesCollector* CreateTablePropertiesCollector(
        rocksdb::TablePropertiesCollectorFactory::Context context) override {
        return new KLLCollector<Record_TYPE, ID_TYPE, DATA_TYPE>(n_, c_, s_, m_);
    }

    const char* Name() const override { return "KLLCollectorFactory"; }

private:
    int n_, s_, m_;
    double c_;
};

#endif