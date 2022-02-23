#pragma once
#include <vector>
#include <string>
#include <iostream>

#include "document.h"

using namespace std::string_literals;

template <typename RandomIt>
class IteratorRange
{
public:

    IteratorRange(RandomIt it1, RandomIt it2)
        : begin_range(it1),
        end_range(it2) {}

    RandomIt begin() const { return begin_range; }
    RandomIt end() const { return end_range; }
    size_t size() const { return end_range - begin_range; }

private:

    RandomIt begin_range;
    RandomIt end_range;
};

template <typename RandomIt>
std::ostream& operator<<(std::ostream& os, const IteratorRange<RandomIt>& ir)
{
    for (auto it = ir.begin(); it != ir.end(); ++it)
    {
        os << *it;
    }
    return os;
}

template <typename RandomIt>
class Paginator
{
public:

    Paginator(RandomIt begin_range, RandomIt end_range, size_t page_size)
    {
        if (page_size < 1)
            throw std::invalid_argument("Page size should be a positive number"s);
        if (end_range == begin_range)
            throw std::invalid_argument("Paginator cannot be initialized with an empty container"s);
        auto page_start = begin_range;
        auto page_end = begin_range;
        int counter = 0;
        for (auto it = begin_range; it != end_range; ++it)
        {
            if (counter == page_size)
            {
                page_end = it;
                pages_.push_back(IteratorRange<RandomIt>{ page_start, page_end });
                page_start = it;
                counter = 0;
            }
            else
                ++counter;
        }
        pages_.push_back(IteratorRange<std::vector<Document>::const_iterator>{ page_start, end_range });
    }

    auto begin() const { return pages_.begin(); }
    auto end() const { return pages_.end(); }
    size_t size() const { return pages_.size(); }
private:
    std::vector<IteratorRange<RandomIt>> pages_;
};

template <typename Container>
auto Paginate(Container& c, size_t page_size)
{
    return Paginator(begin(c), end(c), page_size);
}