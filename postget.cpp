#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "postget.h"

std::string httpResponseData;
using json = nlohmann::json;

size_t http_writeCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t realsize = size * nmemb;
    std::cout << "Response: " << std::string((char *)ptr, realsize) << std::endl;
    httpResponseData.append((char *)ptr, realsize);
    return realsize;
}

void http_post(const std::string &url, const std::string &postData) {
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode res;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_writeCallback);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

std::string http_createJson(float temp, float humid, const std::string &ventStt) {
    std::stringstream jsonStream;
    jsonStream << "{";
    jsonStream << "\"tempDisplay\": " << temp << ", ";
    jsonStream << "\"humiDisplay\": " << humid << ", ";
    jsonStream << "\"ventState\": \"" << ventStt << "\"";
    jsonStream << "}";
    return jsonStream.str();
}

void http_get(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode res;

        httpResponseData.clear();  // Xóa dữ liệu cũ

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_writeCallback);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }
}