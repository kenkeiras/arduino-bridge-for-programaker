#include "secrets.h"
#include <WebSocketsClient.h>
#include "programaker_bridge.hpp"

#include "m5imu.h" // this be included before <M5Stack.h>
#include <M5Stack.h>


#include "setup_wifi.h"

#define FONT_POINT_MULTIPLIER 6
#define MAX_FONT 7
#define MAX_CHARACTERS_COLS 53
#define MAX_CHARACTERS_ROWS 30

#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 320

// Leds
#include <Adafruit_NeoPixel.h>

#define M5STACK_FIRE_NEO_NUM_LEDS 10
#define M5STACK_FIRE_NEO_DATA_PIN 15

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);

void(* resetFunc) (void) = 0; // declare reset function at address 0

void setErrorBars() {
    // Set bars to red
    const int r = 0xff;
    const int g = 0x00;
    const int b = 0x00;

    for (int pixelNumber=0; pixelNumber < M5STACK_FIRE_NEO_NUM_LEDS; pixelNumber++){
        Serial.printf("Setting pixel %i to (%i, %i, %i)\n", pixelNumber, r,g,b);
        pixels.setPixelColor(pixelNumber, pixels.Color(r, g, b));
    }

    pixels.show();
}

void removeBars() {
    pixels.clear();
    pixels.show();
}

void fail() {
    setErrorBars();

    // Crash
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


JSONVar set_left_bar(JSONVar arguments) {
    Serial.println(arguments);

    const int r = as_int(arguments[0]);
    const int g = as_int(arguments[1]);
    const int b = as_int(arguments[2]);

    for (int pixelNumber=(M5STACK_FIRE_NEO_NUM_LEDS / 2); pixelNumber < M5STACK_FIRE_NEO_NUM_LEDS; pixelNumber++){
        Serial.printf("Setting pixel %i to (%i, %i, %i)\n", pixelNumber, r,g,b);
        pixels.setPixelColor(pixelNumber, pixels.Color(r, g, b));
    }

    pixels.show();

    return nullptr;
}

JSONVar set_right_bar(JSONVar arguments) {
    Serial.println(arguments);

    const int r = as_int(arguments[0]);
    const int g = as_int(arguments[1]);
    const int b = as_int(arguments[2]);

    for (int pixelNumber=0; pixelNumber < M5STACK_FIRE_NEO_NUM_LEDS / 2; pixelNumber++){
        pixels.setPixelColor(pixelNumber, pixels.Color(r, g, b));
    }

    pixels.show();

    return nullptr;
}


JSONVar print_line(JSONVar arguments) {
    const char* line = (const char*) arguments[0];
    M5.Lcd.println(line);

    return nullptr;
}


JSONVar set_fullscreen(JSONVar arguments) {

    Serial.print("Setting fullscreen: ");


    auto var = arguments[0];
    auto type = JSON.typeof_(var);

    if (type == "number") {
        auto value = (const int) var;
        int len = ((int) log10(abs(value))) + 1;

        auto font_size = min(53 / len, 7);
        M5.Lcd.setTextSize(font_size);
        int centerY = SCREEN_HEIGHT / 2 - ((font_size * FONT_POINT_MULTIPLIER) / 2);
        int centerX = SCREEN_WIDTH / 2;
        centerX -= (((float)len) / 2) * (font_size * FONT_POINT_MULTIPLIER);

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(centerX, centerY);
        M5.Lcd.println(value);
    }
    else {
        auto value = (const char *) var;
        int len = strlen(value);

        auto font_size = min(53 / len, 7);
        M5.Lcd.setTextSize(font_size);
        int centerY = SCREEN_HEIGHT / 2 - ((font_size * FONT_POINT_MULTIPLIER) / 2);
        int centerX = SCREEN_WIDTH / 2;
        centerX -= (((float)len) / 2) * (font_size * FONT_POINT_MULTIPLIER);

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(centerX, centerY);
        M5.Lcd.println(value);
    }

    return nullptr;
}


JSONVar _get_sensors() {
    float accX = 0.0F;
    float accY = 0.0F;
    float accZ = 0.0F;

    float gyroX = 0.0F;
    float gyroY = 0.0F;
    float gyroZ = 0.0F;

    float pitch = 0.0F;
    float roll  = 0.0F;
    float yaw   = 0.0F;

    float temp = 0.0F;
    int8_t battery = 0.0F;

    M5.IMU.getGyroData(&gyroX,&gyroY,&gyroZ);
    M5.IMU.getAccelData(&accX,&accY,&accZ);
    M5.IMU.getAhrsData(&pitch,&roll,&yaw);
    M5.IMU.getTempData(&temp);
    battery = M5.Power.getBatteryLevel();

    temp = (int) temp; // Truncate temp to integer

    JSONVar value;
    JSONVar gyro;
    gyro["x"] = gyroX;
    gyro["y"] = gyroY;
    gyro["z"] = gyroZ;

    JSONVar acc;
    acc["x"] = accX;
    acc["y"] = accY;
    acc["z"] = accZ;

    JSONVar ahrs;
    ahrs["pitch"] = pitch;
    ahrs["roll"] = roll;
    ahrs["yaw"] = yaw;

    value["gyro"] = gyro;
    value["acc"] = acc;
    value["ahrs"] = ahrs;
    value["temp"] = temp;
    value["battery"] = battery;

    return value;
}

JSONVar get_sensors(JSONVar arguments) {
    return _get_sensors();
}

JSONVar clear_screen(JSONVar arguments) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);

    return nullptr;
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

    getter_def sensor_getter = {
        .id="get_sensors",
        .fun_name="get_sensors",
        .message="Get sensors",
        .arguments=std::list<getter_argument>(),
        .callback=get_sensors,
    };

    operation_argument rgb_argument = {
        .type=INTEGER,
        .default_value="255",
    };

    operation_def set_left_bar_op = {
        .id="set_left_bar",
        .fun_name="set_left_bar",
        .message="Color left bar (r:%1, g:%2, b:%3)",
        .arguments=std::list<operation_argument>({
                rgb_argument,
                rgb_argument,
                rgb_argument,
            }),
        .callback=set_left_bar,
    };

    operation_def set_right_bar_op = {
        .id="set_right_bar",
        .fun_name="set_right_bar",
        .message="Color right bar (r:%1, g:%2, b:%3)",
        .arguments=std::list<operation_argument>({
                rgb_argument,
                rgb_argument,
                rgb_argument,
            }),
        .callback=set_right_bar,
    };

    operation_argument string_argument = {
        .type=STRING,
        .default_value="Hello!",
    };

    operation_def print_line_op = {
        .id="print_line",
        .fun_name="print_line",
        .message="Print line: %1",
        .arguments=std::list<operation_argument>({
                string_argument,
            }),
        .callback=print_line,
    };

    operation_def set_fullscreen_op = {
        .id="set_fullscreen",
        .fun_name="set_fullscreen",
        .message="Set fullscreen: %1",
        .arguments=std::list<operation_argument>({
                string_argument,
            }),
        .callback=set_fullscreen,
    };

    operation_def clear_screen_op = {
        .id="clear_screen",
        .fun_name="clear_screen",
        .message="Clear screen",
        .arguments=std::list<operation_argument>({}),
        .callback=clear_screen,
    };

    bridge = new ProgramakerBridge(webSocket,
                                   BRIDGE_TOKEN,
                                   "M5Stack",
                                   std::list<signal_def>({
                                           sensor_signal,
                                       }),
                                   std::list<getter_def>({
                                           sensor_getter,
                                       }),
                                   std::list<operation_def>({
                                           set_left_bar_op,
                                           set_right_bar_op,
                                           print_line_op,
                                           set_fullscreen_op,
                                           clear_screen_op,
                                       }));
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
    case WStype_DISCONNECTED:
        Serial.printf("[WSc] Disconnected\n");
        fail();
        break;

    case WStype_CONNECTED:
    {
        Serial.printf("[WSc] Connected to url: %s\n",  payload);

        M5.Lcd.println("Connection established!");
        removeBars();

        // webSocket.sendPing():
        tryConfigure();
    }
    break;

    case WStype_TEXT:
        Serial.printf("[WSc] get text: %s [len: %u]\n", payload, length);
        bridge->on_received_text((char*) payload, length);

        // send message to server
        // webSocket.sendTXT("message here");
        break;

    case WStype_BIN:
        Serial.printf("[WSc] get binary length: %u\n", length);
        bridge->on_received_text((char*) payload, length);

        // send data to server
        // webSocket.sendBIN(payload, length);
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

void setup() {
    // Initialize the M5Stack object
    M5.begin();
    /*
      Power chip connected to gpio21, gpio22, I2C device
      Set battery charging voltage and current
      If used battery, please call this function in your project
    */
    M5.Power.begin();

    M5.IMU.Init();

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setTextSize(1);

    pixels.begin();

    pinMode(25, OUTPUT);

//     // Set base
//     Serial.begin();
     Serial.setDebugOutput(true);
// #ifdef GDBDEBUG
//     gdbstub_init();
// #endif

    // Connect wifi
    DEBUG_WEBSOCKETS("Connecting");

    setErrorBars();

    if (!setup_wifi()) {
      M5.Lcd.println("NO CONNECTION, restarting!");
      Serial.println("NO CONNECTION, restarting!");
      delay(1000);
      fail();
    }

#ifdef GDBDEBUG
    delay(5000);
#endif

    connect();
}

void connect() {
    // Establish websocket
    // TCP connection
    if (webSocket != NULL) {
        delete webSocket;
    }

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
