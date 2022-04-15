#pragma once
#include <map>
#include <mutex>
#include <vector>
#include <type_traits>
#include <future>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Bucket
    {
        std::map<Key, Value> map;
        std::mutex mutex;
    };

    struct Access
    {
        std::lock_guard<std::mutex> lg;
        Value& ref_to_value;

        explicit Access(Bucket& b, const Key& k)
            : lg(b.mutex),
            ref_to_value(b.map[k]) {}
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {}

    Access operator[](const Key& key)
    {
        size_t submap_number = size_t(key) % buckets_.size();
        return Access{ buckets_[submap_number], key };
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> result;
        for (auto& bucket : buckets_)
        {
            std::lock_guard lg(bucket.mutex);
            for (const auto& [k, v] : bucket.map)
            {
                result[k] = v;
            }
        }
        return result;
    }

    size_t Size() const
    {
        size_t result = 0;
        for (const Bucket& b : buckets_)
        {
            result += b.map.size();
        }
        return result;
    }

private:
    std::vector<Bucket> buckets_;
};

template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(ExecutionPolicy&& policy, ForwardRange& range, Function function)
{
    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy> ||
        std::is_same_v<typename std::iterator_traits<typename ForwardRange::iterator>::iterator_category, std::random_access_iterator_tag>)
    {
        std::for_each(policy, range.begin(), range.end(), function);
    }
    else
    {
        int part_range = std::thread::hardware_concurrency();
        int part_length = range.size() / part_range;

        auto part_begin = range.begin();
        auto part_end = next(range.begin(), part_length);
        std::vector<std::future<void>> futures;

        for (int counter = 0; counter < part_range; ++counter)
        {
            futures.push_back(async([function, part_begin, part_end] { for_each(part_begin, part_end, function); }));
            part_begin = part_end;
            part_end = (counter == part_range - 2) ? range.end() : next(part_end, part_length);
        }
    }
}