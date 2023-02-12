#pragma once
#include <vector>
#include <iostream>

template <typename It>
class IteratorRange {
public:
    It begin() const {
        return _begin;
    }

    It end() const {
        return _end;
    }

    [[nodiscard]]
    int size() const {
        return distance(_begin, _end);
    }

    IteratorRange(It begin, It end) {
        _begin = begin;
        _end = end;
    }

private:
    It _begin;
    It _end;
};

template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size) {
        while (distance(begin, end) > 0 && distance(begin, end) >= page_size)
        {
            auto range_begin = begin;
            advance(begin, page_size);
            auto range_end = begin;
            _pages.push_back({ range_begin, range_end });
        }
        if (begin != end) _pages.push_back({ begin, end });
    }

    auto begin() const {
        return _pages.begin();
    }

    auto end() const {
        return _pages.end();
    }

private:
    std::vector<IteratorRange<It>> _pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iter>
void PrintRange(Iter range_begin, Iter range_end) {
    for (auto it = range_begin; it != range_end; ++it) {
        std::cout << *it;
    }
}

template <typename It>
std::ostream& operator<<(std::ostream& os, const IteratorRange<It>& range)
{
    PrintRange(range.begin(), range.end());
    return os;
}