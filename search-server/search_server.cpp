#include "search_server.h"
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

SearchServer::SearchServer(const std::string &stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){}
SearchServer::SearchServer(const std::string_view stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings) {
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("Документ с таким id уже существует."s);
    }
    else if (document_id < 0) {
        throw invalid_argument("Документ не может иметь отрицательный id."s);
    }
    else if (!IsValidWord(document)) {
        throw invalid_argument("Содержимое документа содержит недопустимые символы"s);
    }
    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view& word : words) {
        word_to_document_freqs_[std::string{word}][document_id] += inv_word_count;
        words_to_id_[document_id][word_to_document_freqs_.find(word)->first] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, const string_view& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename ExecutionPolicy>
vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, const string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
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

const map<string_view, double, less<>> SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double, std::less<>> empty_map;
    if (!words_to_id_.count(document_id)) {
        return empty_map;
    }
    return words_to_id_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    for (auto &words: words_to_id_.at(document_id)) {
        word_to_document_freqs_[std::string{words.first}].erase(document_id);
    }
    words_to_id_.erase(document_id);
}

MatchedDocuments SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

MatchedDocuments SearchServer::MatchDocument(execution::sequenced_policy policy, const string_view& raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("There is no document with such id");
    }
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Invalid query");
    }
    const auto query = ParseQuery(raw_query);

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&] (auto &word) {
        return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(std::string{word}).count(document_id);})) {
        return { std::vector<std::string_view> {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    if (!matched_words.empty()) {
        auto new_end = std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(),matched_words.begin(),
                                    [&](const auto& plus_word) {
                                        return word_to_document_freqs_.at(std::string{plus_word}).count(document_id);
                                    });
        matched_words.resize(distance(matched_words.begin(), new_end));


        return { matched_words, documents_.at(document_id).status };
    }
    return {std::vector<std::string_view> {}, documents_.at(document_id).status};
}

MatchedDocuments SearchServer::MatchDocument(execution::parallel_policy policy, const std::string_view& raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("There is no document with such id");
    }
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("Invalid query");
    }
    const auto query = ParseQuery(raw_query, true);

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&] (auto &word) {
        return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(std::string{word}).count(document_id);})) {
        return { std::vector<std::string_view> {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    if (!matched_words.empty()) {
        auto new_end = std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(),matched_words.begin(),
                                    [&](const auto& plus_word) {
                                        return word_to_document_freqs_.at(std::string{plus_word}).count(document_id);
                                    });
        matched_words.resize(distance(matched_words.begin(), new_end));

        set<string_view> no_duplications(matched_words.begin(), matched_words.end());

        return { vector<string_view> { no_duplications.begin(), no_duplications.end() }, documents_.at(document_id).status };
    }
    return {std::vector<std::string_view> {}, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
    return std::none_of(word.begin(), word.end(),
                        [](char c) { return c >= '\0' && c < ' '; });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWords(text)) {
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

SearchServer::QueryWord
SearchServer::ParseQueryWord(std::string_view& text) const {
    if (text.empty()) {
        throw std::invalid_argument("Empty text");
    }
    bool is_minus = false;
    if (text.front() == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text.front() == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Parse query error");
    }

    return QueryWord{text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const string_view& text, bool par) const {
    Query result;
    for (const string_view& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(const_cast<string_view &>(word));
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
double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string{word}).size());
}