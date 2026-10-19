#include "bushfire/HttpClient.hpp"
#include <chrono>
#include <cstring>
#include <algorithm>

namespace bushfire {

void HttpClient::globalInit() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void HttpClient::globalCleanup() {
    curl_global_cleanup();
}

HttpClient::HttpClient() {}
HttpClient::~HttpClient() {}

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalBytes = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), totalBytes);
    return totalBytes;
}

size_t HttpClient::headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalBytes = size * nitems;
    HttpResponse* resp = static_cast<HttpResponse*>(userdata);
    std::string header(buffer, totalBytes);

    auto colonPos = header.find(':');
    if (colonPos != std::string::npos) {
        std::string key = header.substr(0, colonPos);
        std::string val = header.substr(colonPos + 1);

        // Trim
        auto trim = [](std::string& str) {
            str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), str.end());
        };
        trim(key);
        trim(val);

        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

        if (lowerKey == "etag") {
            resp->etag = val;
        } else if (lowerKey == "last-modified") {
            resp->lastModified = val;
        }
    }

    return totalBytes;
}

HttpResponse HttpClient::get(const std::string& url, long timeoutSec,
                             const std::string& ifNoneMatchEtag,
                             const std::string& ifModifiedSince) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.errorMessage = "Failed to initialize CURL handle";
        return resp;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json, text/csv, application/xml, */*");
    if (!ifNoneMatchEtag.empty()) {
        std::string h = "If-None-Match: " + ifNoneMatchEtag;
        headers = curl_slist_append(headers, h.c_str());
    }
    if (!ifModifiedSince.empty()) {
        std::string h = "If-Modified-Since: " + ifModifiedSince;
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BushfireMemory/3.0 (C++20; Geospatial Intelligence Platform)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();

    resp.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (res != CURLE_OK) {
        resp.errorMessage = curl_easy_strerror(res);
        resp.statusCode = 0;
    } else {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        resp.statusCode = static_cast<int>(httpCode);
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return resp;
}

HttpResponse HttpClient::post(const std::string& url, const std::string& jsonBody, long timeoutSec) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.errorMessage = "Failed to initialize CURL handle";
        return resp;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BushfireMemory/3.0 (C++20; Geospatial Intelligence Platform)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonBody.length());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();

    resp.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (res != CURLE_OK) {
        resp.errorMessage = curl_easy_strerror(res);
        resp.statusCode = 0;
    } else {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        resp.statusCode = static_cast<int>(httpCode);
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return resp;
}

} // namespace bushfire
