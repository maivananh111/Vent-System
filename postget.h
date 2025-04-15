#ifndef __POSTGET_H__
#define __POSTGET_H__

size_t http_writeCallback(void *ptr, size_t size, size_t nmemb, void *stream);

void http_post(const std::string &url, const std::string &postData);

std::string http_createJson(float temp, float humid, const std::string &ventStt);

void http_get(const std::string &url);

#endif