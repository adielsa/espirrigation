#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include "PubSubClient.h"

#include "secret.h"



/**
 * MQTT
 */
// Host name
const char *ESP_NAME = "ESPrinkler";



WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

//---[ WEB ]---
ESP8266WebServer server(80);


bool shouldRestart = false;
bool otaInProgress = false;
bool sprInProgress = false;

/**
 * PINS
 */

//static const uint8_t LED_BUILTIN = 16;
//static const uint8_t BUILTIN_LED = 16;
//
//static const uint8_t D0   = 16;
//static const uint8_t D1   = 5; //pmwa
//static const uint8_t D2   = 4; //pmwb
//static const uint8_t D3   = 0; //da
//static const uint8_t D4   = 2; //db
//static const uint8_t D5   = 14;
//static const uint8_t D6   = 12;
//static const uint8_t D7   = 13;
//static const uint8_t D8   = 15;
//static const uint8_t D9   = 3;
//static const uint8_t D10 = 1;





void motor_stop(){
    motor_set(0,0,0,0,0);
}


void motor_set(int a0,int a1,int a2,bool a3,int a4) {
    //    digitalWrite(D0, a0);
    digitalWrite(D1, a1);
    digitalWrite(D2, a2);
    digitalWrite(D3, a3);
    digitalWrite(D4, a4);
    //    Serial.print("A0:");
    //    Serial.print(a0);
    Serial.print(" A1:");
    Serial.print(a1);
    Serial.print(" A2:");
    Serial.print(a2);
    Serial.print(" A3:");
    Serial.print(a3);
    Serial.print(" A4:");
    Serial.print(a4);
    Serial.println(" ");
}

void inline valve_set(int d1,int d3) {
    digitalWrite(D1, d1);
    digitalWrite(D3, d3);
}


String valve_state;
int timer;

void openWater() {
    valve_state="open";
    sprInProgress=true;
    valve_set(1,1);
    delay(100);
    valve_set(0,0);

}

void closeWater() {
    valve_state="close";
    sprInProgress=false;
    valve_set(1,0);
    delay(100);
    valve_set(0,0);
    timer=0;
}

void startXMin2(int X) {
    openWater();
    delay(1000*60*X);
    closeWater();
}

void startXMin(int X) {
    openWater();
    timer=10*60*X;
}

void motor_pos() {
    motor_set(0,1,0,1,0);
    delay(100);
    motor_stop();
}

void motor_neg() {
    motor_set(0,1,0,0,0);
    delay(100);
    motor_stop();
}


void handleRoot() {
    server.send(200, "text/plain", "pos,neg, openWater, closeWater! valve:" + valve_state);
}

void setuproot(void){

    server.on("/", handleRoot);

    server.on("/openWater", []() {
            openWater();
            server.send(200, "text/plain", "openWater "+valve_state);
            });

    server.on("/closeWater", []() {
            closeWater();
            server.send(200, "text/plain", "closeWater "+valve_state);
            });

    server.on("/start5Min", []() {
            startXMin(5);
            server.send(200, "text/plain", "start5Min " +valve_state);
            });

    server.on("/start10Min", []() {
            startXMin(10);
            server.send(200, "text/plain", "start10Min "+valve_state);
            });

    // on off

    server.on("/pos", []() {
            motor_pos();
            server.send(200, "text/plain", "Pos" + valve_state);
            });

    server.on("/neg", []() {
            motor_neg();
            server.send(200, "text/plain", "Neg "+valve_state);
            });

}

void initGpio() {
    // prepare GPIO5 relay 1
    pinMode(D1, OUTPUT);
    digitalWrite(D1, 0);

    pinMode(D2, OUTPUT);
    digitalWrite(D2, 0);

    pinMode(D3, OUTPUT);
    digitalWrite(D3, 0);

    pinMode(D4, OUTPUT);
    digitalWrite(D4, 0);

    valve_state="Unkown";

}


void initSerial() {
    Serial.begin(115200);
    Serial.println("");


    if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }
}

void initWiFi() {
    WiFi.begin(ssid, password);
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void mqttConnect() {
    while (!mqttClient.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (mqttClient.connect(ESP_NAME)) {
            Serial.printf("Connected to %s\n", MQTT_HOST);
            mqttPublish("{\"event\":\"connected\"}", MQTT_OUT_TOPIC);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void initMQTT() {
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttConnect();
}



void setup(void){

    //  // prepare Motor Output Pins
    //  pinMode(D0, OUTPUT);
    //  digitalWrite(D0, 0);

    initGpio();
    closeWater();

    initSerial();
    initOTA();
    initWiFi();

    setuproot();
    server.begin();
    Serial.println("HTTP server started");

    initMQTT();

}

void loop(void){
    if (shouldRestart) {
        delay(500);
        ESP.restart();
    }

    server.handleClient();
    delay(100);
    if (timer >= 1) {
        if (timer == 1) {
            closeWater();
        }
        timer--;
    }

    // Handle wifi connection
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        initWiFi();
        return;
    }


    // Handle OTA
    if (!sprInProgress) {
        ArduinoOTA.handle();
        if (otaInProgress) {
            return;
        }
    }
    // Handle MQTT
    if (!mqttClient.connected()) {
        mqttConnect();
    }
    mqttClient.loop();
}
