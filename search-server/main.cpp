#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int SET_PRECISION = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{
                                   ComputeAverageRating(ratings),
                                   status
                           });
    }
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const{
        return FindTopDocuments(raw_query, [&status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    template <typename DocumentPredicate>
    vector <Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < SET_PRECISION) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
                text,
                is_minus,
                IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData found_document_info = documents_.at(document_id);
                if (document_predicate(document_id, found_document_info.status, found_document_info.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                                                document_id,
                                                relevance,
                                                documents_.at(document_id).rating
                                        });
        }
        return matched_documents;
    }
};
/*
   Подставьте сюда вашу реализацию макросов
    ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template <typename Container>
void Print(ostream& out, const Container& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
}

template<typename Element>
ostream& operator <<(ostream& os, const set<Element> container){
    os << "{";
    Print (os, container);
    os << "}";
    return os;
}

template<typename Element>
ostream& operator <<(ostream& os, const vector<Element> container){
    os << "[";
    Print (os, container);
    os << "]";
    return os;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& os, const map<Key, Value>& container) {
    bool is_first = true;
    os << '{';
    for (const auto& para:container){
        if (!is_first){
            os << ", "s;
        }
        os << para.first << ": "<< para.second;
        is_first = false;
    }
    os << '}';
    return os;
}

template <typename Func>
void RunTestImpl(const Func& func, const string& func_name) {
    func();
    cerr << func_name <<" OK"<<endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
// -------- Начало модульных тестов поисковой системы ----------

// Тест на стоп-слова.
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Тест на минус-слова.
void TestExcludeMinusWordsFromAddedDocumentContent(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "cat and dog in the home"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id2);
    }
}

// Тест на возращение всех поисковых слов.
void TestFindDocumentWithSearchWords(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const int doc_id2 = 43;
    const string content2 = "dog in the city"s;
    const vector<int> ratings = {1, 2, 3};
    vector <string> split_words_doc_no_stop = {"cat", "city"};
    {
        SearchServer server;
        server.SetStopWords("in the");
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        vector<string> returned_doc_words = get<vector<string>>(server.MatchDocument("cat -dog in the city"s, doc_id));
        ASSERT_EQUAL_HINT(returned_doc_words, split_words_doc_no_stop, "Found docs");
        DocumentStatus doc_status = get<DocumentStatus>(server.MatchDocument("cat -dog in the city"s, doc_id));
        ASSERT_HINT(doc_status == DocumentStatus::ACTUAL, "Checking Document status");
        vector<string> returned_doc_words2 = get<vector<string>>(server.MatchDocument("cat -dog in the city"s, doc_id2));
        ASSERT_HINT(returned_doc_words2.empty(), "Docs with minus-words");
    }
}

// Тест на обычный поиск и добавление документов.
void TestSimpleSearchAndAddingDocuments (){
    SearchServer server;
    ASSERT_HINT(server.GetDocumentCount() == 0, "No documents");
    const int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 2;
    const string content2 = "cat and dog in the home"s;
    {
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.GetDocumentCount() == 2, "+documents");
    }
}

// Тест на сортировку по релевантности.
void TestSortRelevance(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "cat and dog in the city"s;
    const int doc_id3 = 44;
    const string content3 = "cat in the box"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in the city"s);
        ASSERT_HINT(found_docs[0].relevance>found_docs[1].relevance, "Relevance test");
        ASSERT(found_docs[0].relevance>found_docs[2].relevance);
    }
}

// Тест подсчета рейтинга.
void TestAverageRating(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat dog"s);
        int average_rating=0;
        for (const int rating: ratings){
            average_rating += rating;
        }
        average_rating = average_rating / static_cast<int>(ratings.size());
        ASSERT_EQUAL(found_docs[0].rating, average_rating);
    }
}

// Тест на предикат, задаваемый пользователем.
void TestPredicate(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    const int doc_id2 = 43;
    const string content2 = "cat in the downtown";
    const vector <int> ratings2 = {3,2,6};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);

        const auto found_docs = server.FindTopDocuments("cat in"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 1; });
        ASSERT_HINT(found_docs.size() == 1, "Odd ids");
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id2);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat in"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
        ASSERT_HINT(found_docs.size() == 1, "Banned docs");
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        const auto found_docs = server.FindTopDocuments("cat in"s, [](int document_id, DocumentStatus status, int rating) { return rating == 3; });
        ASSERT_HINT(found_docs.size() == 1, "Rating=3");
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id2);
    }
}

// Тест на заданный статус.
void TestStatus(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id2 = 43;
    const string content2 = "cat and dog in the home"s;
    const vector<int> ratings2 = {3, 3, 3};

    const int doc_id3 = 44;
    const string content3 = "cat and dog in the black box"s;
    const vector<int> ratings3 = {4, 4, 4};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(doc_id3, content3, DocumentStatus::REMOVED, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT(found_docs[0].id == 43);
        const auto found_docs2 = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1);
        ASSERT(found_docs2[0].id == 44);
        const auto found_docs3 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_HINT(found_docs3.empty(), "No such status");
    }
}

// Тест на вычисление релевантности.
void TestCountingRelevansIsCorrect(){
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    const int doc_id2 = 43;
    const string content2 = "cat and dog in the home"s;
    const vector<int> ratings2 = {3, 3, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat dog city"s, DocumentStatus::ACTUAL);

        map<string, map<int, double>> word_to_document_freqs;
        const vector <string> split_words_doc = SplitIntoWords (content);
        const vector <string> split_words_doc2 = SplitIntoWords (content2);
        const double inv_word_count = 1.0 / split_words_doc.size();
        const double inv_word_count2 = 1.0 / split_words_doc2.size();
        const vector<string> split_words_query = SplitIntoWords ("cat dog city"s);
        map <int, double> document_to_relevance;
        for (const string word : split_words_doc) {
            word_to_document_freqs[word][doc_id] += inv_word_count;
        }
        for (const string word : split_words_doc2) {
            word_to_document_freqs[word][doc_id2] += inv_word_count2;
        }
        for (const string& word : split_words_query) {
            if (word_to_document_freqs.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = log(server.GetDocumentCount() * 1.0 / word_to_document_freqs.at(word).size());;
            for (const auto [document_id, term_freq] : word_to_document_freqs.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
        double relevance = document_to_relevance[doc_id];
        double relevance2 = document_to_relevance[doc_id2];

        ASSERT_EQUAL_HINT(abs(found_docs[0].relevance - relevance), SET_PRECISION, "Testing relevance counting");
        ASSERT_EQUAL(abs(found_docs[1].relevance - relevance2), SET_PRECISION);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestFindDocumentWithSearchWords);
    RUN_TEST(TestSimpleSearchAndAddingDocuments);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestAverageRating);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestCountingRelevansIsCorrect);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}