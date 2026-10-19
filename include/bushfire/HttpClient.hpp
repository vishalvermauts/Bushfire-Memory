#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>

namespace bushfire {

struct HttpResponse {
    int statusCode{0};
    std::string body;
    long latencyMs{0};
    std::string etag;
    std::string lastModified;
    std::string errorMessage;

    bool isSuccess() const {
        return statusCode >= 200 && statusCode < 300;
    }

    bool isNotModified() const {
        return statusCode == 304;
    }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Perform an HTTPS GET request with latency measurement
    HttpResponse get(const std::string& url, 
                     long timeoutSec = 12,
                     const std::string& ifNoneMatchEtag = "",
                     const std::string& ifModifiedSince = "");

    // Perform an HTTPS POST request with JSON payload
    HttpResponse post(const std::string& url, 
                      const std::string& jsonBody,
                      long timeoutSec = 12);

    static void globalInit();
    static void globalCleanup();

private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
};

} // namespace bushfire
