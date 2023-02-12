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

#include "paginator.tpp"