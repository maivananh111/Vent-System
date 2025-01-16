
#include "func.h"



Timer::Timer(void){
    this->isRunning = false;
}

Timer::~Timer(void) {
    stop();
}

void Timer::setCallback(const std::function<void()>& callback) {
    this->callback = callback;
}

void Timer::start(int durationMs) {
    stop(); 
    isRunning = true;

    timerThread = std::thread([=]() {
        auto startTime = std::chrono::steady_clock::now();
        while (isRunning) {
            auto elapsedTime = std::chrono::steady_clock::now() - startTime;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count() >= durationMs) {
                if (isRunning && callback) { 
                    callback();
                }
                break; 
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
        }
    });
}


void Timer::stop(void) {
    isRunning = false;
    if (timerThread.joinable()) {
        timerThread.join();
    }
}



int vent_find_element_index(std::vector<end_device_t*>& vec, std::string deveui) {
    auto it = std::find_if(vec.begin(), vec.end(), [&deveui](end_device_t* element) {
        return element && element->deveui == deveui; 
    });

    if (it != vec.end())
        return std::distance(vec.begin(), it); 

    return -1; 
}


std::vector<std::string> split_string(std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<uint8_t> base64_decode(const std::string& base64_str) {
    size_t len = base64_str.size();
    size_t padding = 0;
    if (base64_str[len - 1] == '=') padding++;
    if (base64_str[len - 2] == '=') padding++;

    size_t decoded_len = (len * 3) / 4 - padding;

    std::vector<uint8_t> decoded_data(decoded_len);

    int result_len = EVP_DecodeBlock(decoded_data.data(), reinterpret_cast<const unsigned char*>(base64_str.c_str()), len);
    if (result_len < 0) {
        std::cerr << "Base64 decoding failed!" << std::endl;
        return {}; 
    }

    return decoded_data;
}

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::vector<unsigned char> encoded_data(EVP_ENCODE_LENGTH(data.size()));

    int result_len = EVP_EncodeBlock(encoded_data.data(), data.data(), data.size());

    return std::string(reinterpret_cast<char*>(encoded_data.data()), result_len);
}

