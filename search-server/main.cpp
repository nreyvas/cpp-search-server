//#include "search_server.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;


// Подставьте вашу реализацию класса SearchServer сюда
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;


string ReadLine()
{
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

vector<string> SplitIntoWords(const string& text)
{
    vector<string> words;
    string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
            word += c;
    }
    if (!word.empty())
        words.push_back(word);

    return words;
}

struct Document
{
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer		// MAIN CLASS
{
private:

    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };


    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;  // string - word, int - id, double - TF
    map<int, DocumentData> documents_;  // int = id

    bool IsStopWord(const string& word) const
    {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const
    {
        vector<string> words;
        for (const string& word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
                words.push_back(word);
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings)
    {
        if (ratings.empty())
            return 0;
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    Query ParseQuery(const string& text) const
    {
        Query query;
        for (const string& word : SplitIntoWords(text))
        {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                    query.minus_words.insert(query_word.data);
                else
                    query.plus_words.insert(query_word.data);
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate_function) const
    {
        map<int, double> document_to_relevance; // int - id, double - relevance
        for (const string& word : query.plus_words)
        {
            auto it = word_to_document_freqs_.find(word);
            if (it == word_to_document_freqs_.end())
                continue;
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : it->second)
            {
                const DocumentData& document_data = documents_.at(document_id);
                if (predicate_function(document_id, document_data.status, document_data.rating))
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
        for (const string& word : query.minus_words)
        {
            auto it = word_to_document_freqs_.find(word);
            if (it == word_to_document_freqs_.end())
                continue;
            for (const auto [document_id, _] : it->second)
                document_to_relevance.erase(document_id);
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
public:

    void SetStopWords(const string& text)
    {
        for (const string& word : SplitIntoWords(text))
            stop_words_.insert(word);
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words)
            word_to_document_freqs_[word][document_id] += inv_word_count;
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query, Predicate predicate_function) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate_function);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON)
                    return lhs.rating > rhs.rating;
                else
                    return lhs.relevance > rhs.relevance; });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        return matched_documents;
    }


    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const
    {
        return FindTopDocuments(raw_query, [status]
        (int document_id, DocumentStatus out_status, int rating)
            {return out_status == status; });
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;
            if (word_to_document_freqs_.at(word).count(document_id))
                matched_words.push_back(word);
        }
        for (const string& word : query.minus_words)
        {
            if (word_to_document_freqs_.count(word) == 0)
                continue;
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }
};

//---------------------------------------------------------------------------------------------------


// Подставьте сюда вашу реализацию макросов
// ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
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

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TF>
void RunTestImpl(TF TestFunction, const string& function_name)
{
    TestFunction();
    cerr << function_name << " OK"s << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}


// Разместите код остальных тестов здесь
void TestExcludeMinusWords()
{
    const int doc_id = 42;
    const string content = "a flying lynx in a turquoise pijama"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-turquoise"s).empty());
    }
}

void TestMatchingDocuments()
{
    const int doc_id = 42;
    const string content = "a flying lynx in a turquoise pijama"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [matched_words, status] = server.MatchDocument("a turquoise lynx in a gray pijama"s, 42);
        ASSERT_EQUAL(matched_words.size(), 5);
    }
}

void TestRelevanceSorting()
{
    const int doc_id_0 = 0;
    const int doc_id_1 = 1;
    const int doc_id_2 = 2;
    const string content_0 = "a flying lynx in a turquoise pijama"s;
    const string content_1 = "a cat in the city"s;
    const string content_2 = "a howling lynx in the gotham pijama city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id_0, content_0, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

    auto found_docs = server.FindTopDocuments("cat in blue pijama");
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 0);
    ASSERT_EQUAL(found_docs[2].id, 2);

    found_docs = server.FindTopDocuments("a cat in the city");
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 2);
    ASSERT_EQUAL(found_docs[2].id, 0);
}

void TestRating()
{
    const vector<int> ids = { 0, 1, 2, 3, 4, 5, 6 };
    const vector<int> ratings_0 = { 0, 0, 0 };  // 0
    const vector<int> ratings_1{};  // 0
    const vector<int> ratings_2 = { -120, 845, 0 }; // 241
    const vector<int> ratings_3 = { 25, 12 };   // 18
    const vector<int> ratings_4 = { 5 };    // 5
    const vector<int> ratings_5 = { 7, -25, 45, 87, 18544 };    // 3731
    const vector<int> ratings_6 = { -100, 100 };    // 0
    const string content_0 = "flying lynx in a turquoise pijama"s;
    const string content_1 = "cat in the city"s;
    const string content_2 = "howling lynx in the gotham pijama city"s;
    const string content_3 = "crawling owl under stone bridge"s;
    const string content_4 = "scared snake with impressive eyes"s;
    const string content_5 = "purple monkey with golden collar"s;
    const string content_6 = "pink elephant surrounded by dancing mushrooms"s;
    const string query_0 = "flying lynx in a stone pijama"s;
    const string query_1 = "scared monkey with dancing golden bear"s;
    SearchServer server;
    server.AddDocument(ids[0], content_0, DocumentStatus::ACTUAL, ratings_0);
    server.AddDocument(ids[1], content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(ids[2], content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(ids[3], content_3, DocumentStatus::ACTUAL, ratings_3);
    server.AddDocument(ids[4], content_4, DocumentStatus::ACTUAL, ratings_4);
    server.AddDocument(ids[5], content_5, DocumentStatus::ACTUAL, ratings_5);
    server.AddDocument(ids[6], content_6, DocumentStatus::ACTUAL, ratings_6);

    auto found_docs = server.FindTopDocuments(query_0);
    ASSERT_EQUAL(found_docs[0].rating, 0);
    ASSERT_EQUAL(found_docs[1].rating, 241);
    ASSERT_EQUAL(found_docs[2].rating, 18);
    ASSERT_EQUAL(found_docs[3].rating, 0);

    found_docs = server.FindTopDocuments(query_1);
    ASSERT_EQUAL(found_docs[0].rating, 3731);
    ASSERT_EQUAL(found_docs[1].rating, 5);
    ASSERT_EQUAL(found_docs[2].rating, 0);
}

void TestPredicateFunctions()
{
    SearchServer server;
    const vector<int> ids = { 0, 1, 2, 3, 4 };
    const vector<int> ratings_0 = { 0, 0, 0 };
    const vector<int> ratings_1{};
    const vector<int> ratings_2 = { -120, -845, 0 };
    const vector<int> ratings_3 = { -25, 12 };
    const vector<int> ratings_4 = { 5 };
    const string content_0 = "flying lynx in a turquoise pijama"s;
    const string content_1 = "lynx in the city with blue eyes"s;
    const string content_2 = "howling lynx in the gotham collar city"s;
    const string content_3 = "crawling lynx under stone bridge"s;
    const string content_4 = "scared linx with impressive eyes"s;
    server.AddDocument(ids[0], content_0, DocumentStatus::ACTUAL, ratings_0);
    server.AddDocument(ids[1], content_1, DocumentStatus::BANNED, ratings_1);
    server.AddDocument(ids[2], content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(ids[3], content_3, DocumentStatus::REMOVED, ratings_3);
    server.AddDocument(ids[4], content_4, DocumentStatus::BANNED, ratings_4);

    auto found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return rating < 0; });
    ASSERT_EQUAL(found_docs.size(), 2);
    ASSERT_EQUAL(found_docs[0].id, 2);
    ASSERT_EQUAL(found_docs[1].id, 3);
    found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return rating == 99; });
    ASSERT(found_docs.empty());

    found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return id == 2; });
    ASSERT_EQUAL(found_docs.size(), 1);
    ASSERT_EQUAL(found_docs[0].id, 2);
    found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return id == -45; });
    ASSERT(found_docs.empty());

    found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
    ASSERT_EQUAL(found_docs.size(), 2);
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 4);
    found_docs = server.FindTopDocuments("blue lynx with howling stone collar",
        [](int id, DocumentStatus status, int rating) { return status == DocumentStatus::IRRELEVANT; });
    ASSERT(found_docs.empty());
}

void TestStatus()
{
    SearchServer server;
    const vector<int> ids = { 0, 1, 2, 3, 4 };
    const vector<int> ratings = { 1, 2, 3 };
    const string content_0 = "flying lynx in a turquoise pijama"s;
    const string content_1 = "lynx in the city with blue eyes"s;
    const string content_2 = "howling lynx in the gotham collar city"s;
    const string content_3 = "crawling lynx under stone bridge"s;
    const string content_4 = "scared linx with impressive eyes"s;
    server.AddDocument(ids[0], content_0, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(ids[1], content_1, DocumentStatus::BANNED, ratings);
    server.AddDocument(ids[2], content_2, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(ids[3], content_3, DocumentStatus::REMOVED, ratings);
    server.AddDocument(ids[4], content_4, DocumentStatus::BANNED, ratings);

    auto found_docs = server.FindTopDocuments("blue lynx with howling stone collar"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs.size(), 2);
    ASSERT_EQUAL(found_docs[0].id, 1);
    ASSERT_EQUAL(found_docs[1].id, 4);
    found_docs = server.FindTopDocuments("blue lynx with howling stone collar"s, DocumentStatus::IRRELEVANT);
    ASSERT(found_docs.empty());
}

void TestRelevanceCalculating()
{
    const double EPSILON = 1e-5;
    const int doc_id_0 = 15;
    const int doc_id_1 = 42;
    const int doc_id_2 = 8;
    const string content_0 = "a flying lynx in a turquoise pijama"s;
    const string content_1 = "a cat in the city"s;
    const string content_2 = "a howling lynx in the gotham pijama city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id_0, content_0, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

    auto found_docs = server.FindTopDocuments("cat in blue pijama");
    double correct_relevance_0 = 0.219722;
    double correct_relevance_1 = 0.057924;
    double correct_relevance_2 = 0.050683;
    ASSERT(abs(found_docs[0].relevance - correct_relevance_0) < EPSILON);
    ASSERT(abs(found_docs[1].relevance - correct_relevance_1) < EPSILON);
    ASSERT(abs(found_docs[2].relevance - correct_relevance_2) < EPSILON);

    found_docs = server.FindTopDocuments("dog under stone bridge");
    ASSERT(found_docs.empty());

    found_docs = server.FindTopDocuments("a cat in the city");
    correct_relevance_0 = 0.381908;
    correct_relevance_1 = 0.101366;
    correct_relevance_2 = 0;
    ASSERT(abs(found_docs[0].relevance - correct_relevance_0) < EPSILON);
    ASSERT(abs(found_docs[1].relevance - correct_relevance_1) < EPSILON);
    ASSERT(abs(found_docs[2].relevance - correct_relevance_2) < EPSILON);
}



// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestRelevanceCalculating);
    RUN_TEST(TestRating);
    RUN_TEST(TestPredicateFunctions);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevanceSorting);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}