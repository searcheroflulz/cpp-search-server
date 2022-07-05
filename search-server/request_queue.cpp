#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server): server(search_server){}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
        // напишите реализацию
        const auto& result = server.FindTopDocuments(raw_query, status);
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
        } else if(time < min_in_day_){
            query.emptiness = false;
            requests_.push_back(query);
        }
        return result;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
        // напишите реализацию
        const auto& result = server.FindTopDocuments(raw_query);
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
        } else if(time < min_in_day_){
            query.emptiness = false;
            requests_.push_back(query);
        }
        return result;
}

int RequestQueue::GetNoResultRequests() const {
        // напишите реализацию
        int fail = 0;
        for (QueryResult result: requests_){
            if (result.emptiness)
                fail++;
        }
        return fail;
}