#include <execution>
#include <functional>
#include <numeric>

#include "process_queries.h"


std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const std::string& query) {
        return search_server.FindTopDocuments(query);
    });
    return result;
}

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {
    std::vector<Document> result;
    auto documents = ProcessQueries(search_server, queries);

    result = std::reduce(std::execution::par, documents.begin(), documents.end(), result, [](std::vector<Document> left, std::vector<Document> right) {
        std::vector<Document> temp;
        temp.insert(temp.end(), left.begin(), left.end());
        temp.insert(temp.end(), right.begin(), right.end());
        return temp;
    });

    return result;
}