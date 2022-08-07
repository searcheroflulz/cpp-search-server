#include "search_server.h"
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_to_id_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);

}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> empty_map;

    if (!words_to_id_.count(document_id)) {
        return empty_map;
    }

    return words_to_id_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    for (auto &words: words_to_id_.at(document_id)) {
        word_to_document_freqs_[words.first].erase(document_id);
    }
    words_to_id_.erase(document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy policy, const string& raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy policy, const std::string& raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("There is no document with such id");
    }
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Invalid query");
    }
    const auto query = ParseQuery(raw_query, true);

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&] (auto &word) {
        return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id);})) {
        return { std::vector<std::string> {}, documents_.at(document_id).status };
    }

    std::vector<std::string> matched_words(query.plus_words.size());

    if (!matched_words.empty()) {
        auto new_end = std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(),matched_words.begin(),
                                    [&](const auto& plus_word) {
            return word_to_document_freqs_.at(plus_word).count(document_id);
        });
        matched_words.resize(distance(matched_words.begin(), new_end));

        set<string> no_duplications(matched_words.begin(), matched_words.end());

        return { vector<string> { no_duplications.begin(), no_duplications.end() }, documents_.at(document_id).status };
    }
    return {std::vector<std::string> {}, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    rating_sum = std::accumulate(ratings.begin(), ratings.end(), rating_sum);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const string& text, bool par) const {
    Query result;
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (!par) {
        sort(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());

        sort(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());

    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}