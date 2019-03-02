/**
 * Yi Multi Remote
 * 
 * Triggers 2 Yi action cameras in sync (for poor man's VR rig)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <OneButton.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include "main.h"
#include "config.h"

OneButton button(BTN_PIN, true);
DNSServer dnsServer;
AsyncWebServer webServer(80);
WiFiClient client1;
WiFiClient client2;

uint8_t token1;
uint8_t token2;

bool videoMode = true;          // true when in video mode, false when in photo mode
bool videoRec = false;          // true when video recording in progress
bool stationsConnected = false; // true when both cameras are connected to AP
bool clientsConnected = false;  // true when both cameras are connected to server
bool actionTriggered = false;   // Indicates if action has been triggered by the web server


/**
 * SETUP
 */
void setup() {
    Serial.begin(115200);
    Serial.println();

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);


    /**
     * START WIFI ACCESS POINT
     */

    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(AP_SSID, AP_PWD, 6)) {
        Serial.println(F("Unable to start Access Point"));
        restartESP(10000);
    }

    Serial.printf_P(PSTR("Access point \"%s\" started with IP "), AP_SSID);
    Serial.println(WiFi.softAPIP());


    /**
     * DNS SERVER SETUP
     */

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", AP_IP);


    /**
     * WEB SERVER SETUP
     */

    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS begin failed");
        restartESP(10000);
    }

    webServer.on("/action", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println(F("webServer.on /action"));
        if (clientsConnected) {
            // This is a workaround as we can't use yield (or any
            // function that uses it) inside the callback
            actionTriggered = true;
        }
        request->send(204);
    });

    webServer.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println(F("webServer.on /mode"));
        if (clientsConnected) {
            AsyncWebParameter *type = request->getParam("type");
            AsyncWebParameter *param = request->getParam("param");
            setMode(type->value().c_str(), param->value().c_str());
        }
        request->send(204);
    });

    webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    webServer.onNotFound([](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html");
    });

    webServer.begin();
    Serial.println("HTTP server started");


    /**
     * ONEBUTTON SETUP
     */

    button.setClickTicks(BTN_CLICK_DELAY_MS);
    button.setPressTicks(BTN_LPRESS_DELAY_MS);

    // Either capture photo or toggle video recording on click
    button.attachClick([]() {
        Serial.println(F("Button click!"));
        if (clientsConnected) {
            action();
        }
    });

    // Alternate between photo and video mode on double click
    button.attachDoubleClick([]() {
        Serial.println(F("Button double click!"));
        if (clientsConnected) {
            videoMode = !videoMode;
            if (videoMode) {
                setMode("rec_mode", DEFAULT_VIDEO_MODE);
            } else {
                setMode("capture_mode", DEFAULT_PHOTO_MODE);
            }
        }
    });

    // Restart ESP on long press
    button.attachLongPressStart([]() {
        Serial.println(F("Button long press!"));
        restartESP();
    });
}


/**
 * LOOP
 */
void loop() {

    if (!stationsConnected) {

        // Yi cameras are supposed to be the first 2 devices connected to the AP
        // Remember not to connect your smartphone until the initialization sequence has ended
        // Reconnection is not supported in this version : if a camera disconnects, all devices
        // must be switched off and the initialization sequence restarted from the beginning.

        if (WiFi.softAPgetStationNum() == 2) {
            stationsConnected = true;
            Serial.println(F("All stations connected to AP"));

            // Turns the LED on to signal both cameras are connected
            digitalWrite(LED_PIN, LOW);

            // Client 1 initialization
            if (!client1.connect(CLIENT1_IP, CLIENT_PORT)) {
                Serial.println(F("Unable to connect client1!"));
                restartESP(10000);
            }

            Serial.println(F("Client1 connected!"));
            getToken(client1, token1);

            // Client 2 initialization
            if (!client2.connect(CLIENT2_IP, CLIENT_PORT)) {
                Serial.println(F("Unable to connect client2!"));
                restartESP(10000);
            }

            Serial.println(F("Client2 connected!"));
            getToken(client2, token2);

            // Both cameras are fully initialized \o/
            clientsConnected = true;

            // Turns them to video mode
            setMode("rec_mode", DEFAULT_VIDEO_MODE);
            videoMode = true;

            // Turns the LED off and beep to signal end of initialization sequence
            // Now you are free to connect your smartphone to the AP
            digitalWrite(LED_PIN, HIGH);
            tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
        }
    }

    // OneButton handler
    button.tick();

    // DNS handler
    dnsServer.processNextRequest();

    // Action handler (triggered in web server callback)
    if (actionTriggered) {
        action();
        actionTriggered = false;
    }
}


/**
 * Get token
 */
void getToken(WiFiClient &client, uint8_t &token) {

    Serial.println(F("Sending token request..."));

    client.println(F(R"({"msg_id":257,"token":0 })"));

    // Get response
    // !FIXME Handle mixed responses
    // {"rval":0,"msg_id":257,"param":1,"model":"Z18","rtsp":"rtsp://192.168.42.1/live"}{"msg_id":7,"type":"battery","param":"75"}

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 20000) {
            Serial.println(F(">>> Client Timeout !"));
            return;
        }
        yield();
    }

    String response = client.readStringUntil('\r');
    Serial.printf_P(PSTR("Client response: %s\n"), response.c_str());

    StaticJsonBuffer<512> jsonInBuffer;
    JsonObject &json = jsonInBuffer.parseObject(response);

    if (!json.success()) {
        Serial.println(F("Unable to parse response"));
        return;
    }

    if (json.containsKey(F("param"))) {
        token = json["param"].as<uint8_t>();
        Serial.printf_P(PSTR("Token: %d\n"), token);
    }
}


/**
 * Capture photo
 */
void capturePhoto() {
    fireYiMessage(YI_MSG_CAPTURE_PHOTO);
}


/**
 * Toggle video recording
 * 
 * Video recording is toggled on or off depending on videoRec flag
 */
void toggleVideo() {
    if (videoRec) {

        // First, beep...
        tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
        unsigned long m = millis();
        while (millis() - m < 1000) {
            yield();
        }

        // ...then stop video
        fireYiMessage(YI_MSG_STOP_VIDEO_REC);

    } else {

        // First, start video...
        fireYiMessage(YI_MSG_START_VIDEO_REC);

        // ...then beep
        unsigned long m = millis();
        while (millis() - m < 2000) {
            yield();
        }
        tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);

    }
    videoRec = !videoRec;
}


/**
 * Send message to Yi, without considering the response
 * 
 * uint16_t id Message id
 */
void fireYiMessage(uint16_t id) {

    char msg[100];
    const char *format = R"({"token":%d,"msg_id":%d})";

    snprintf(msg, sizeof(msg), format, token1, id);
    client1.print(msg);

    snprintf(msg, sizeof(msg), format, token2, id);
    client2.print(msg);

    // Trying to improve camera sync by finalizing the message at the same time
    client1.println();
    client2.println();
}


/**
 * Either trigger photo capture or start/stop video recording
 */
void action() {
    if (videoMode) {
        toggleVideo();
    } else {
        capturePhoto();
    }
}


/**
 * Set camera mode
 *
 * @param type
 * @param param
 */
void setMode(const char *type, const char *param) {

    char msg[100];
    const char *format = R"({"token":%d,"msg_id":2,"type":"%s","param":"%s"})";

    snprintf(msg, sizeof(msg), format, token1, type, param);
    client1.println(msg);

    snprintf(msg, sizeof(msg), format, token2, type, param);
    client2.println(msg);

    videoMode = (strcmp(type, "rec_mode") == 0);
}


/**
 * Restarts ESP after delayMs milliseconds
 *
 * @param delayMs
 */
void restartESP(uint16_t delayMs) {

    uint8_t i;
    for (i = 0; i < 3; i++) {
        tone(BUZZER_PIN, BUZZER_FREQUENCY, BUZZER_DURATION);
        delay(100);
    }

    unsigned long m = millis();
    while (true) {
        if (millis() - m > delayMs)
            ESP.restart();
        yield();
    }
}