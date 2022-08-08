#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> duplicates;
    std::map<std::set<std::string>, int> set_of_words_to_id;
    for (const int document_id: search_server) {
        std::set<std::string> words_set;
        for (auto& words: search_server.GetWordFrequencies(document_id)) {
            words_set.insert(std::string{words.first});
        }
        if (set_of_words_to_id.count(words_set)) {
            std::cout << "Found duplicate document id " << document_id << std::endl;
            duplicates.push_back(document_id);
        }
        else set_of_words_to_id[words_set] = document_id;
    }

    for (auto id: duplicates) {
        search_server.RemoveDocument(id);
    }
}