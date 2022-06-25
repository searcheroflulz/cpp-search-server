#pragma once
#include <string>
#include <vector>

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, const uint32_t page_size) {
    	int count_page;
        assert(range_end >= range_begin && page_size > 0);
        auto new_begin = range_begin;
        for (auto documents_left = distance(range_begin, range_end); documents_left > 0;) {
            const size_t current_page_size = min(page_size, documents_left);
            const Iterator current_page_end = next(new_begin, current_page_size);
            pages_.push_back({new_begin, current_page_end});
            documents_left -= current_page_size;
            new_begin = current_page_end;
        }
    }
    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }
    
    std::vector<std::string> GetPages() {
    return pages_;
    }
    
private:
    std::vector<std::string> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}