#include <iostream>
#include <mosquitto.h>
#include <sstream>
#include <vector>
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "func.h"


#define DOWNLINK_FPORT      5U
#define PAIR_TIMEOUT_MS     (30*1000U)

typedef struct {
    std::string appid;
    std::string deveui;
    std::string devname;
    uint8_t devidx;
    std::vector<uint8_t> data_hex;
} event_data_t;

static std::vector<end_device_t *> vec_end_devices;
static end_device_t *sensor_pairing;
static end_device_t *vent_pairing;
static bool pairing = false;
static Timer pair_timeout_timer;


void handle_topic(struct mosquitto *mosq, const std::vector<std::string> &tokens, const std::string &payload);
void mqttc_on_connect(struct mosquitto *mosq, void *userdata, int state);
void mqttc_on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message);
int schedule_downlink(struct mosquitto *mosq, event_data_t *evt_data, std::vector<uint8_t> down_data_hex); /* ??? */
void handle_uplink_data(struct mosquitto *mosq, event_data_t *evt_data);
void pair_timeout_timer_handler(void);

void vent_set_state(std::string deveui, uint8_t state);
void sensor_display_env_data(event_data_t *evt_data);
void sensor_change_set_temperature(std::string deveui, float set);





int main(void) {
    pair_timeout_timer.setCallback(pair_timeout_timer_handler);

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





void handle_uplink_data(struct mosquitto *mosq, event_data_t *evt_data) {
    if (!evt_data->data_hex.empty()) {
        std::cout << "[DATA] |";
        for (uint8_t byte : evt_data->data_hex)
            std::cout   << std::setw(2)           
                        << std::setfill('0')      
                        << std::hex
                        << static_cast<int>(byte) 
                        << "|";
        std::cout << "is ";

        std::vector<uint8_t> dldata;

        dldata.push_back(evt_data->data_hex[0]);
        dldata.push_back(evt_data->data_hex[1] + 1);

        if (evt_data->data_hex[0] == VENT_DEVICE) {
            std::cout << "Vent device" << std::endl;
            end_device_t *pdev = vec_end_devices[evt_data->devidx];
	    vent_pairing = pdev;

            switch (evt_data->data_hex[1]){
                case VENT_CMD_POLL_CTRL_CMD: {
                    std::cout << "[COMMAND] Poll control command" << std::endl;
                    dldata.push_back(pdev->state);
                    schedule_downlink(mosq, evt_data, dldata);
                } break;

                case VENT_CMD_PAIR_REQ: {
                    std::cout << "[COMMAND] Pair request" << std::endl;
                    
                    if (pdev->pair_dev == nullptr) {
                        if (pairing == true) {
                            pair_timeout_timer.stop();
                            pairing = false;
                            pdev->pair_dev = sensor_pairing;
                            sensor_pairing->pair_dev = pdev;
                            std::cout << "Pair successful" << std::endl;
                        }
                        else
                            std::cout << "No sensor devices are waiting to be paired.." << std::endl;
                    }
                    else
                        std::cout << "This device has been paired with vent " << pdev->pair_dev->deveui << std::endl;   

                } break;
                
                default:
                break;
            };
        }
        else if (evt_data->data_hex[0] == SENSOR_DEVICE) {
            std::cout << "Sensor device" << std::endl;
            end_device_t *pdev = vec_end_devices[evt_data->devidx];

            switch (evt_data->data_hex[1]){
                case SENSOR_CMD_ENVDATA: {
                    std::cout << "[COMMAND] Env data" << std::endl;

                    float temp = (float)((evt_data->data_hex[2] << 8) | evt_data->data_hex[3]) / 100.0;
                    float humi = (float)((evt_data->data_hex[4] << 8) | evt_data->data_hex[5]) / 100.0;

                    pdev->battlevel = evt_data->data_hex[6];
                    pdev->temperature = temp;
                    pdev->humidity = humi;

                    sensor_display_env_data(evt_data);

                    uint16_t tempset = (uint16_t)(pdev->temperature_set * 100);
                    dldata.push_back((tempset >> 8)&0xFF);
                    dldata.push_back((tempset >> 0)&0xFF);
                    schedule_downlink(mosq, evt_data, dldata);
                
		/* So sanh voi nhietj do dat
		 * vent_pairing khong phai null
		 * vent_set_state(thiet bi pdev->pair_dev);
		 * */
		} break;

                case SENSOR_CMD_PAIR_REQ: {
                    std::cout << "[COMMAND] Pair request" << std::endl;
                    
                    if (pdev->pair_dev == nullptr) {
                        if (pairing == false) {
                            std::cout << "Start pair" << std::endl;
                            sensor_pairing = pdev;
                            pairing = true;
                            pair_timeout_timer.start(PAIR_TIMEOUT_MS);
                        }
                        else 
                            std::cout << "The pairing process is in progress, please wait for completion." << std::endl;
                    }
                    else
                        std::cout << "This device has been paired with vent " << pdev->pair_dev->deveui << std::endl;

                } break;

                case SENSOR_CMD_PAIR_STAT: {
                    std::cout << "[COMMAND] Pair status request" << std::endl;
                    dldata.push_back((vec_end_devices[evt_data->devidx]->pair_dev)? 0 : 1);
                    schedule_downlink(mosq, evt_data, dldata);
                } break;

                default:
                break;
            };
        }
    } 
}


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

            if (evt_data.devidx = vent_find_element_index(vec_end_devices, evt_data.deveui) < 0) {
                end_device_t *new_dev = new end_device_t;
                new_dev->deveui = evt_data.deveui;
                new_dev->temperature_set = 30.0;
                vec_end_devices.push_back(new_dev);
                evt_data.devidx = vec_end_devices.size() - 1;
            }
            if (json_data.contains("data")) {
                std::string data_base64 = json_data["data"];
                evt_data.data_hex = base64_decode(data_base64);
            }


            if (event_type == "up") {
                std::cout << "\r\n\r\n[EVENT] Event: " << event_type
                                    << ", Eui: " << evt_data.deveui
                                    << ", Name: " << evt_data.devname << std::endl;

                handle_uplink_data(mosq, &evt_data);

                std::cout << "==========================================================================" << std::endl;
            } 
        } 
    } 
}

int schedule_downlink(struct mosquitto *mosq, event_data_t *evt_data, std::vector<uint8_t> down_data_hex) {
    std::string topic  = "application/" + evt_data->appid + "/device/" + evt_data->deveui + "/command/down";

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

void pair_timeout_timer_handler(void) {
    pairing = false;
    std::cout << "Pairing failed, no device A is available to pair." << std::endl;
}



void mqttc_on_connect(struct mosquitto *mosq, void *userdata, int state) {
    if (state == 0) {
        std::cout << "Connected to MQTT broker!" << std::endl;
        mosquitto_subscribe(mosq, nullptr, "#", 0); 
    } else {
        std::cerr << "Failed to connect, return code: " << state << std::endl;
    }
}

void mqttc_on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    std::string topic = message->topic;
    std::string payload(static_cast<char*>(message->payload), message->payloadlen);
    std::vector<std::string> topic_dir = split_string(topic, '/');

    handle_topic(mosq, topic_dir, payload);
}




void vent_set_state(std::string deveui, uint8_t state) {
    uint8_t idx = vent_find_element_index(vec_end_devices, deveui);
    if (idx >= 0) {
        vec_end_devices[idx]->state = state;
        std::cout << "Device " << deveui << " set vent state to " << ((state)? "ON":"OFF") << std::endl;
    }
}

void sensor_display_env_data(event_data_t *evt_data) {
    end_device_t *pdev = vec_end_devices[evt_data->devidx];

    std::cout << "= " << evt_data->deveui << " =" << std::endl;
    std::cout << "==== TEMP: " << pdev->temperature << " ====" << std::endl;
    std::cout << "==== HUMI: " << pdev->humidity << " ====" << std::endl;
    std::cout << "==== BATT: " << pdev->battlevel << " ====" << std::endl;
}

void sensor_change_set_temperature(std::string deveui, float set) {
    uint8_t idx = vent_find_element_index(vec_end_devices, deveui);
    if (idx >= 0) {
        vec_end_devices[idx]->temperature_set = set;
        std::cout << "Device " << deveui << " set temperature has been changed to " << set << std::endl;
    }
}



