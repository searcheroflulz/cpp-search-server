#pragma once
#include "search_server.h"
#include <vector>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const auto& result = server.FindTopDocuments(raw_query, document_predicate);
        time++;
        QueryResult query;

        if (result.empty()) {
            query.emptiness = true;
            if (time > min_in_day_)
                requests_.pop_front();
            requests_.push_back(query);
        }
        else if (time > min_in_day_) {
            requests_.pop_front();
            query.emptiness = false;
            requests_.push_back(query);
        }else if(time < min_in_day_){
            query.emptiness = false;
            requests_.push_back(query);
        }
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool emptiness = false;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int time = 0;
    const SearchServer& server;
};