#include "document.h"
#include <iostream>
#include <sstream>
using namespace std;

Document::Document(int id, double relevance, int rating) : id(id), relevance(relevance), rating(rating) {}

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

string PrintDocumentToString(const Document& document) {
    ostringstream out;
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return out.str();
}