#include "httpreq.hpp"
#include <string_view>
#include <charconv>
#include <iostream>
using namespace std;

namespace {

constexpr string_view trim(string_view sv) {
    size_t start = sv.find_first_not_of(" \t\r\n");
    if (start == string_view::npos) return {};
    size_t end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

HttpMethod parseMethod(string_view method) {
    if      (method == "GET")    return HttpMethod::GET;
    else if (method == "POST")   return HttpMethod::POST;
    else if (method == "PUT")    return HttpMethod::PUT;
    else if (method == "DELETE") return HttpMethod::DEL;
    else                         return HttpMethod::POST;  // default fallback
}

size_t parseSizeT(string_view sv) {
    size_t result = 0;
    size_t start = sv.find_first_not_of(" \t");
    if (start == string_view::npos) return 0;
    sv = sv.substr(start);
    std::from_chars(sv.data(), sv.data() + sv.size(), result);
    return result;
}
}

void HttpRequest::parseQueryParams(const string& query) {
    if (query.empty()) return;
    string_view qv(query);
    size_t pos = 0;
    while (pos < qv.size()) {
        size_t eq = qv.find('=', pos);
        if (eq == string_view::npos) break;
        string_view key = qv.substr(pos, eq - pos);

        size_t amp = qv.find('&', eq + 1);
        size_t valEnd = (amp == string_view::npos) ? qv.size() : amp;
        string_view value = qv.substr(eq + 1, valEnd - (eq + 1));

        queryParams[string(key)] = string(value);

        pos = (amp == string_view::npos) ? qv.size() : amp + 1;
    }
}

void HttpRequest::parseHeaders(const string& req) {
    if (req.empty()) return;
    string_view rv(req);

    size_t lineEnd = rv.find("\r\n");
    if (lineEnd == string_view::npos) return;
    string_view reqLine = rv.substr(0, lineEnd);

    size_t space1 = reqLine.find(' ');
    if (space1 == string_view::npos) return;
    string_view methodSv = trim(reqLine.substr(0, space1));
    method = parseMethod(methodSv);

    size_t pathStart = space1 + 1;
    size_t space2 = reqLine.find(' ', pathStart);
    string_view fullPath;
    if (space2 == string_view::npos) {
        fullPath = trim(reqLine.substr(pathStart));
    } else {
        fullPath = trim(reqLine.substr(pathStart, space2 - pathStart));
    }

    size_t qpos = fullPath.find('?');
    if (qpos != string_view::npos) {
        path  = string(fullPath.substr(0, qpos));
        query = string(fullPath.substr(qpos + 1));
        parseQueryParams(query);
    } else {
        path  = string(fullPath);
        query.clear();
    }

    size_t headerPos = lineEnd + 2;
    contentLength = 0;
    contentType.clear();

    while (headerPos < rv.size()) {
        size_t nextLine = rv.find("\r\n", headerPos);
        if (nextLine == string_view::npos) break;
        if (nextLine == headerPos) break;

        string_view headerLine = rv.substr(headerPos, nextLine - headerPos);

        size_t colon = headerLine.find(':');
        if (colon != string_view::npos) {
            string_view key   = trim(headerLine.substr(0, colon));
            string_view value = trim(headerLine.substr(colon + 1));

            if (key == "Content-Length") {
                contentLength = parseSizeT(value);
            } else if (key == "Content-Type") {
                contentType = string(value);
            }
        }

        headerPos = nextLine + 2;
    }
}