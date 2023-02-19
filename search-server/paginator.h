#pragma once
#include <vector>
#include <iostream>

template <typename It>
class IteratorRange {
public:
    It begin() const;
    It end() const;
    [[nodiscard]] int size() const;
    IteratorRange(It begin, It end);

private:
    It _begin;
    It _end;
};

template <typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size);
    auto begin() const;
    auto end() const;
private:
    std::vector<IteratorRange<It>> _pages;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size);

template <typename Iter>
void PrintRange(Iter range_begin, Iter range_end);

template <typename It>
std::ostream& operator<<(std::ostream& os, const IteratorRange<It>& range);


//Def

template <typename It>
It IteratorRange<It>::begin() const {
    return _begin;
}

template <typename It>
It IteratorRange<It>::end() const {
    return _end;
}

template <typename It>
int IteratorRange<It>::size() const {
    return distance(_begin, _end);
}

template <typename It>
IteratorRange<It>::IteratorRange(It begin, It end) {
    _begin = begin;
    _end = end;
}

template <typename It>
Paginator<It>::Paginator(It begin, It end, size_t page_size) {
    while (distance(begin, end) > 0 && distance(begin, end) >= page_size)
    {
        auto range_begin = begin;
        advance(begin, page_size);
        auto range_end = begin;
        _pages.push_back({ range_begin, range_end });
    }
    if (begin != end) _pages.push_back({ begin, end });
}

template <typename It>
auto Paginator<It>::begin() const {
    return _pages.begin();
}

template <typename It>
auto Paginator<It>::end() const {
    return _pages.end();
}

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
std::ostream& operator<<(std::ostream& os, const IteratorRange<It>& range) {
    PrintRange(range.begin(), range.end());
    return os;
}