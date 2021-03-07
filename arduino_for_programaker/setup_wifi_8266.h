#ifdef DEVICE_ESP8266

#include "secrets.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

ESP8266WiFiMulti WiFiMulti;

bool setup_wifi() {
    WiFiMulti.addAP(WIFI_SSID, WIFI_PSK);

    for (int i = 0; i < 10; i++) {
        if (WiFiMulti.run() == WL_CONNECTED) {
          return true;
        }
        delay(500);
        DEBUG_WEBSOCKETS(".");
    }

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());

    return (WiFiMulti.run() == WL_CONNECTED);
}

#endif
