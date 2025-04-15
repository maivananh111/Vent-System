#include <iostream>
#include <mosquitto.h>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <vector>
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <unistd.h>
#include <pthread.h>

#include "func.h"


#define DOWNLINK_FPORT      5U
#define PAIR_TIMEOUT_MS     (30*1000U)

typedef struct {
    std::string appid;
    std::string deveui;
    std::string devname;
    int devidx;
    std::vector<uint8_t> data_hex;
} event_data_t;

using json = nlohmann::json;

static std::vector<end_device_t *> vec_end_devices;
static end_device_t *sensor_pairing;
static end_device_t *vent_pairing;
static bool pairing = false;
static Timer pair_timeout_timer;

const std::string postUrl = "http://192.168.1.11:1880/sensor-data";
const std::string getUrl = "http://192.168.1.11:1880/get-sensor-data";

void handle_topic(struct mosquitto *mosq, const std::vector<std::string> &tokens, const std::string &payload);
void mqttc_on_connect(struct mosquitto *mosq, void *userdata, int state);
void mqttc_on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
int schedule_downlink(struct mosquitto *mosq, event_data_t *evt_data, std::vector<uint8_t> down_data_hex); /* ??? */
void handle_uplink_data(struct mosquitto *mosq, event_data_t *evt_data);

std::string test_json_get = R"(
    {
        "chairId": "1",
        "deviceId": "ba84ca62714f81fc",
        "status": "0",
        "customer": "Viktor Axelsen",
        "chairRule": "1.5",
        "timeStart": "2025-01-01 01:01",
        "timeEnd": "2025-01-01 01:05",
        "currentTime": "2025-01-01 02:30"
    }
)";

std::string test_json_post = R"(
    {
        "chairId": "1",
        "deviceId": "ba84ca62714f81fc",
        "status": "0",
        "battery": "90",
        "latitude": "101.23264",
        "longitude": "-33.54657465"
    }
)";

std::vector<uint8_t> downlink_payload_test;

std::tm parse_time(const std::string& time_str) {
    std::tm tm = {};
    std::istringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    return tm;
}

uint32_t calculate_duration(const std::string& start, const std::string& end) {
    std::tm start_tm = parse_time(start);
    std::tm end_tm = parse_time(end);

    time_t start_time = mktime(&start_tm);
    time_t end_time = mktime(&end_tm);

    return static_cast<uint32_t>(difftime(end_time, start_time) / 60); // phút
}

void build_downlink_payload(const std::string& json_str, std::vector<uint8_t>& payload) {
    json data = json::parse(json_str);

    uint32_t chairId = std::stoul(data["chairId"].get<std::string>());
    std::string customer = data["customer"].get<std::string>();
    std::string timeStart = data["timeStart"].get<std::string>();
    std::string timeEnd = data["timeEnd"].get<std::string>();

    uint32_t duration = calculate_duration(timeStart, timeEnd);
    std::cout << "duration: " << duration << std::endl;

    // 1. Push chairId (1 bytes - Little Endian)
    payload.push_back((chairId >> 0) & 0xFF);

    // 2. Push duration (4 bytes - Little Endian)
    payload.push_back((duration >> 0) & 0xFF);
    payload.push_back((duration >> 8) & 0xFF);
    payload.push_back((duration >> 16) & 0xFF);
    payload.push_back((duration >> 24) & 0xFF);

    // 3. Push customer name (30 bytes, padding nếu ngắn)
    const size_t MAX_NAME_LEN = 30;
    for (size_t i = 0; i < MAX_NAME_LEN; ++i) {
        if (i < customer.size()) {
            payload.push_back(static_cast<uint8_t>(customer[i]));
        } else {
            payload.push_back(0);  // padding bằng null byte
        }
    }
}

void print_payload_hex(const std::vector<uint8_t>& payload) {
    std::cout << "Downlink Payload (hex): ";
    for (uint8_t byte : payload) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << "";
    }
    std::cout << std::endl;
}
//------------------------------------------------------------//
std::string bytesToHexString(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t byte : data) {
        oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)byte;
    }
    return oss.str();
}

float bytesToFloat(const uint8_t* bytes) {
    float value;
    std::memcpy(&value, bytes, sizeof(float));
    return value;
}

std::string generateJsonFromData(const std::vector<uint8_t>& data) {
    if (data.size() != 10) {
        throw std::runtime_error("Invalid data length! Expect 10 bytes.");
    }

    uint8_t chairId = data[0];

    float latitude = bytesToFloat(&data[1]);
    float longitude = bytesToFloat(&data[5]);

    uint8_t battery = data[9];

    std::ostringstream json;

    json << "{\n";
    json << "    \"chairId\": \"" << (int)chairId << "\",\n";
    json << "    \"deviceId\": \"ba84ca62714f81fc\",\n";
    json << "    \"status\": \"1\",\n";
    json << "    \"battery\": \"" << (int)battery << "\",\n";
    json << "    \"latitude\": \"" << latitude << "\",\n";
    json << "    \"longitude\": \"" << longitude << "\"\n";
    json << "}";

    return json.str();
}


//------------------------------------------------------------//
void delay_ms(uint16_t milisecond) {
    usleep(1000 * milisecond);
}

int main(void) {

    auto json_obj = nlohmann::json::parse(test_json_get);

    // std::string chairId = json_obj["chairId"];
    // std::string timeStart = json_obj["timeStart"];
    
    // std::cout << "Chair ID: " << chairId << std::endl;
    // std::cout << "Time Start: " << timeStart << std::endl;


    // chairId (convert string "123" -> uint8_t)
    // std::vector<uint8_t> downlink_payload_test;
    build_downlink_payload(test_json_get, downlink_payload_test);

    print_payload_hex(downlink_payload_test);


    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq) {
        std::cerr << "Failed to create Mosquitto client!" << std::endl;
        return 1;
    }
    mosquitto_connect_callback_set(mosq, mqttc_on_connect);
    mosquitto_message_callback_set(mosq, mqttc_on_message);

    const char *host = "localhost";
    int port = 1883;
    if (mosquitto_connect(mosq, host, port, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to broker!" << std::endl;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}

// void *Thread_WebServer(void *arg) {

//     while (1) {
//         sleep (3);
//         std::cout << "\nSending GET request..." << std::endl;
//         sendGetRequest(getUrl);

//         try {
//             jsonResponse = json::parse(httpResponseData);
//             receivedTemp = jsonResponse["tempSet"];
//             receivedHumidity = jsonResponse["humidSet"];
//             // std::string receivedVentState = jsonResponse["ventState"];
//             httpResponseData.clear();

//             std::cout << "\nParsed GET response:" << std::dec << std::endl;
//             std::cout << "Temperature: " << std::dec << receivedTemp << std::endl;
//             std::cout << "Humidity: " << std::dec << receivedHumidity << std::endl;
//             // std::cout << "Vent State: " << receivedVentState << std::endl;
//         } 
//         catch (json::parse_error &e) {
//             std::cerr << "JSON parsing error: " << e.what() << std::endl;
//         }
//     }
// }

void handle_topic(struct mosquitto *mosq, const std::vector<std::string> &topic_dir, const std::string &payload) {
    if (topic_dir.size() >= 4) {
        if (topic_dir[0] == "application" && topic_dir[4] != "command") {
            /* Get event information */
            nlohmann::json json_data = nlohmann::json::parse(payload);
            std::string event_type = topic_dir[5]; 

            event_data_t evt_data;
            evt_data.appid = topic_dir[1];     
            if (json_data.contains("deviceInfo")) {                
                evt_data.deveui = json_data["deviceInfo"]["devEui"];
                evt_data.devname = json_data["deviceInfo"]["deviceName"];
            }
            evt_data.devidx = vent_find_element_index(vec_end_devices, evt_data.deveui);
            if (evt_data.devidx < 0) {
                end_device_t *new_dev = new end_device_t;
                new_dev->deveui = evt_data.deveui;
                new_dev->temperature_set = 30.0;
                vec_end_devices.push_back(new_dev);
                evt_data.devidx = vec_end_devices.size() - 1;
            }

            if (json_data.contains("data")) {
                std::string data_base64 = json_data["data"];
                std::cout << "======" << std::endl;
                std::cout << "data_base64: " << data_base64 << std::endl;
                evt_data.data_hex = base64_decode(data_base64);
                std::string hex_str(evt_data.data_hex.begin(), evt_data.data_hex.end());
                std::cout << "evt_data.data_hex: " << hex_str << std::endl;
                for (uint8_t byte : evt_data.data_hex) {
                    printf("%02X ", byte);
                }
                printf("\n");

                std::string json = generateJsonFromData(evt_data.data_hex);
                std::cout << "Generated JSON:\n" << json << std::endl;

                // std::cout << "evt_data.data_hex: " << evt_data.data_hex << std::endl;
            }


            if (event_type == "up") {
                std::cout << "\r\n\r\n[EVENT] Event: " << event_type
                                    << ", Eui: " << evt_data.deveui
                                    << ", Name: " << evt_data.devname << std::endl;

                // std::vector<uint8_t> downlink_payload = {0xA1, 0xB2, 0xC3, 0xD4};
                schedule_downlink(mosq, &evt_data, downlink_payload_test);
                // handle_uplink_data(mosq, &evt_data);

                std::cout << "==========================================================================" << std::endl;
            } 
        } 
    } 
}

int schedule_downlink(struct mosquitto *mosq, event_data_t *evt_data, std::vector<uint8_t> down_data_hex) {
    std::string topic  = "application/" + evt_data->appid + "/device/" + evt_data->deveui + "/command/down";
    std::cout << "topic downlink: " << topic << std::endl;
    std::string base64_str = base64_encode(down_data_hex);
    std::ostringstream json;
    json    << "{"
                << "\"devEui\": \"" << evt_data->deveui << "\","
                << "\"confirmed\": false,"
                << "\"fPort\": " << DOWNLINK_FPORT << ","
                << "\"data\": \"" << base64_str << "\""
            << "}";
    std::string json_str = json.str();
    if (mosquitto_publish(mosq, nullptr, topic.c_str(), json_str.length(), json_str.c_str(), 0, 0) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to publish message!" << std::endl;
        return -1;
    } 
    return 0;
}

void mqttc_on_connect(struct mosquitto *mosq, void *userdata, int state) {
    if (state == 0) {
        std::cout << "Connected to MQTT broker!" << std::endl;
        mosquitto_subscribe(mosq, nullptr, "#", 0); 
    } else {
        std::cerr << "Failed to connect, return code: " << state << std::endl;
    }
}


void printVector(const std::vector<std::string>& vec) {
    std::cout << "Topic_dir: ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

void mqttc_on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    std::string topic = message->topic;
    std::cout << "Receive Topic: " << topic << std::endl;
    std::string payload(static_cast<char*>(message->payload), message->payloadlen);
    std::vector<std::string> topic_dir = split_string(topic, '/');
    // printVector(topic_dir);
    // std::cout << "payload: " << payload << std::endl;
    handle_topic(mosq, topic_dir, payload);
}