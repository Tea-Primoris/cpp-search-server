#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
private:

    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> lock;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket) : lock(bucket.mutex), ref_to_value(bucket.map[key]) {

        }
    };

    explicit ConcurrentMap(size_t bucket_count) : buckets_(bucket_count) {

    }

    Access operator[](const Key& key) {
        Bucket& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard<std::mutex> lock(mutex);
            ordinary_map.insert(map.begin(), map.end());
        }
        return ordinary_map;
    }

    void erase(const Key& key) {
        Bucket& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        std::lock_guard<std::mutex> lock(bucket.mutex);
        bucket.map.erase(key);
    }

    [[nodiscard]] size_t size() const {
        size_t size = 0;
        for (const Bucket& bucket : buckets_) {
            size += bucket.map.size();
        }
        return size;
    }

private:
    std::vector<Bucket> buckets_;
};