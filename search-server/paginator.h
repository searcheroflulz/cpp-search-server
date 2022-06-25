#pragma once
#include <string>
#include <vector>
#include <cassert>

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, const uint32_t page_size) {
        int count_page;
        assert(range_end >= range_begin && page_size > 0);
        if (page_size > distance(range_begin, range_end)) {
            std::string page;
            for (auto it = range_begin; it != range_end; ++it) {
                page +=PrintDocumentToString(*it);
            }
            pages_.push_back(page);
        }
        else if (distance(range_begin, range_end) % page_size == 0) {
            count_page = distance(range_begin, range_end) / page_size;
            auto it_begin = range_begin;
            auto it_end = it_begin + page_size;
            for(size_t i = 0; i < count_page; ++i)
            {
                std::string page;
                for (auto it = it_begin; it < it_end; ++it) {
                    page +=PrintDocumentToString(*it);
                }
                pages_.push_back(page);
                it_begin += page_size;
                it_end += page_size;
            }
        } else {
            count_page = distance(range_begin, range_end) / page_size + 1;
            auto it_begin = range_begin;
            auto it_end = it_begin + page_size;
            for(size_t i = 0; i < count_page; ++i)
            {
                std::string page;
                for (auto it = it_begin; it < it_end; ++it) {
                    page +=PrintDocumentToString(*it);
                }
                pages_.push_back(page);
                it_begin += page_size;
                if (it_end + page_size > range_end) {
                    it_end = range_end;
                } else {
                    it_end += page_size;
                }
            }
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