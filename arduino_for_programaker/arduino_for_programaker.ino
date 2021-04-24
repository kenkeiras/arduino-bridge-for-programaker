#include "secrets.h"
#include <WebSocketsClient.h>
#include "programaker_bridge.hpp"


// Uncomment this to enable debugging with GDB
// #define GDBDEBUG

#ifdef GDBDEBUG
#include <GDBStub.h>
#endif

#define DEVICE_ESP8266
#include "setup_wifi_8266.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

void(* resetFunc) (void) = 0; // declare reset function at address 0

void fail() {
  resetFunc();
}

int as_int(JSONVar var) {
    auto type = JSON.typeof_(var);
    if (type == "number") {
        return (const int) var;
    }

    auto str = (const char*) var;
    return atoi(str);
}

WebSocketsClient *webSocket;
ProgramakerBridge *bridge = NULL;

void tryConfigure() {
    // Configure when connected
    signal_argument single_variable_argument = {
        .arg_type=VARIABLE,
        .type=SINGLE,
    };

    signal_def sensor_signal = {
        .id="on_sensor_signal",
        .fun_name="on_sensor_signal",
        .key="on_sensor_signal",
        .message="On sensor update. Set %1",
        .arguments=std::list<signal_argument>({
                single_variable_argument,
            }),
        .save_to={
            .index=0
        }
    };

    bridge = new ProgramakerBridge(webSocket,
                             BRIDGE_TOKEN,
                             "ESP8266",
                             std::list<signal_def>({
                                     sensor_signal,
                                 }),
                             std::list<getter_def>({
                                 }),
                             std::list<operation_def>({
                                 }));
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
    case WStype_DISCONNECTED:
    {
        Serial.printf("[WSc] Disconnected\n");
        fail();
    }
    break;

    case WStype_CONNECTED:
    {
        Serial.printf("[WSc] Connected to url: %s\n",  payload);

        tryConfigure();
    }
    break;

    case WStype_TEXT:
        Serial.printf("[WSc] get text: %s [len: %u]\n", payload, length);
        bridge->on_received_text((char*) payload, length);

        break;

    case WStype_BIN:
        Serial.printf("[WSc] get binary length: %u\n", length);
        bridge->on_received_text((char*) payload, length);

        break;

    case WStype_ERROR:
        // Error
        Serial.printf("[WSc] get error length: %u\n", length);
        break;

        // Fragmented transmissions
    case WStype_FRAGMENT_TEXT_START:
        Serial.printf("[WSc] get fragment text start length: %u\n", length);
        break;
    case WStype_FRAGMENT_BIN_START:
        Serial.printf("[WSc] get fragment bin start length: %u\n", length);
        break;
    case WStype_FRAGMENT:
        Serial.printf("[WSc] get fragment length: %u\n", length);
        break;
    case WStype_FRAGMENT_FIN:
        Serial.printf("[WSc] get fragment fin length: %u\n", length);
        break;

        // Ping-pong
    case WStype_PING:
        Serial.printf("[WSc] get ping length: %u\n", length);
        break;
    case WStype_PONG:
        Serial.printf("[WSc] get pong length: %u\n", length);
        break;

        // Anything else
    default:
        Serial.printf("[WSc] Unknown type: %i", type);
    }
}


#define NUM 10000
int count = 0;
int to_go = NUM;
int last_sound = 0; // As is mapped from several readings, it has to be saved

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0; // We consider timezone=UTC+0
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
    Serial.begin(9600);
    Serial.setDebugOutput(true);

#ifdef GDBDEBUG
    gdbstub_init();
#endif

    // Connect wifi
    DEBUG_WEBSOCKETS("Connecting");

    if (!setup_wifi()) {
      Serial.println("NO CONNECTION, restarting!");
      delay(1000);
      fail();
    }


#ifdef GDBDEBUG
    delay(5000);
#endif

    timeClient.begin();

    connect();
}

void connect() {
    // Establish websocket
    // TCP connection
    if (webSocket != NULL) {
        delete webSocket;
    }

    timeClient.update();

    struct timeval realtime;
    realtime.tv_sec = timeClient.getEpochTime();
    realtime.tv_usec = 0;
    settimeofday(&realtime, NULL);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    Serial.printf("=> Current time: %i/%i/%i %i:%i:%i\n", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    webSocket = new WebSocketsClient();
#ifdef USE_SSL
    webSocket->beginSslWithCA(ENDPOINT_HOST, ENDPOINT_PORT, ENDPOINT_PATH, ENDPOINT_CA_CERT);
#else
    webSocket->begin(ENDPOINT_HOST, ENDPOINT_PORT, ENDPOINT_PATH);
#endif
    webSocket->onEvent(webSocketEvent);
    webSocket->setReconnectInterval(5000);
    webSocket->enableHeartbeat(15000, 15000, 2);
}

#define SENSOR_NUM 50
int sensor_to_go = SENSOR_NUM;

JSONVar _get_sensors() {
  return JSONVar("ping");
}

void update_sensor_signal() {
    sensor_to_go--;
    if (sensor_to_go == 0) {
        JSONVar value = _get_sensors();
        bridge->send_signal("on_sensor_signal", value);

        sensor_to_go = SENSOR_NUM;
    }

}

// the loop function runs over and over again forever
void loop() {
    if (bridge != NULL) {
      bridge->loop();
      update_sensor_signal();
      delay(10);
    }
    else {
      webSocket->loop();
    }
}
