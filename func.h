
#ifndef __FUNC_H__
#define __FUNC_H__


#include <iostream>
#include <mosquitto.h>
#include <sstream>
#include <vector>
#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>



enum {
    OFF,
    ON,
};
enum {
    VENT_DEVICE     = 0x01,
    SENSOR_DEVICE   = 0x02,
};

enum {
    SENSOR_CMD_ENVDATA      = 0x10,
    SENSOR_CMD_PAIR_REQ     = 0x20,
    SENSOR_CMD_PAIR_STAT    = 0x30,

    VENT_CMD_POLL_CTRL_CMD  = 0x10,
    VENT_CMD_PAIR_REQ       = 0x20,
};

struct end_device;

typedef struct end_device {
    std::string deveui      = "";       /* 8-bytes device EUI */
    uint8_t state           = OFF;      /* Vent control state */
    float temperature_set   = 0.0;      /* Sensor temperature set in degrees Celsius */
    float temperature       = 0.0;      /* Sensor temperature in degrees Celsius */
    float humidity          = 0.0;      /* Sensor humidity value in percent */
    uint8_t battlevel       = 0.0;      /* Device battery level [00;FF] */
    uint8_t index           = 0;        /* Device index in vector */
    end_device *pair_dev   = nullptr;
} end_device_t;


class Timer {
public:
    Timer(void);
    ~Timer(void);
    void setCallback(const std::function<void()>& callback);
    void start(int durationMs);
    void stop(void);

private:
    std::atomic<bool> isRunning;         
    std::thread timerThread;            
    std::function<void()> callback = nullptr;
};



std::vector<std::string> split_string(std::string &str, char delimiter);

std::vector<uint8_t> base64_decode(const std::string& base64_str);
std::string base64_encode(const std::vector<uint8_t>& data);

int vent_find_element_index(std::vector<end_device_t *>& vec, std::string deveui);


#endif

