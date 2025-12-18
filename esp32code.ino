/******************************************************************
 * AI-IoT Theft Detection System for ESP32 with MQTT
 * ----------------------------------------------------------------
 * This code receives parsed current data from an Arduino via Serial,
 * calculates theft, and publishes the data to an MQTT broker.
 * It also listens for relay control commands from the dashboard.
 * * Version: 6.0 (Serial Communication with Arduino)
 * By:Rahul Warade
 ******************************************************************/

// --- Libraries ---
#include <WiFi.h>
#include <WiFiClientSecure.h> // For secure MQTT
#include <PubSubClient.h>      // For MQTT
#include <ArduinoJson.h>       // For creating JSON data
#include <HardwareSerial.h>    // For communicating with Arduino

// --- WiFi Settings ---
const char* WIFI_SSID = "Airtel_Zerotouch";
const char* WIFI_PASSWORD = "Airtel@123";

// --- HiveMQ MQTT Broker Settings ---
const char* MQTT_BROKER = "469ca625449a40b28d35c6dbec5aaa71.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883; // Secure Port
const char* MQTT_USER = "admin";
const char* MQTT_PASS = "Pass@123";

// --- MQTT Topics ---
const char* MQTT_TOPIC_DATA = "iot/theft/data";
const char* MQTT_TOPIC_CONTROL = "iot/theft/control";

// --- Hardware Pins ---
const int RELAY_HOUSE_1 = 14;
const int RELAY_HOUSE_2 = 27;
const int RELAY_HOUSE_3 = 26;

// --- System Settings ---
const float THEFT_THRESHOLD = 0.5; // Amps difference to trigger theft status

// --- Initialize Clients ---
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// Initialize Serial2 for communication with Arduino (RX2=16, TX2=17)
HardwareSerial ArduinoSerial(2);

void setup_wifi() {
    delay(10);
    Serial.print("\nConnecting to WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);

    if (String(topic) == MQTT_TOPIC_CONTROL) {
        if (message == "1on") digitalWrite(RELAY_HOUSE_1, LOW);
        else if (message == "1off") digitalWrite(RELAY_HOUSE_1, HIGH);
        else if (message == "2on") digitalWrite(RELAY_HOUSE_2, LOW);
        else if (message == "2off") digitalWrite(RELAY_HOUSE_2, HIGH);
        else if (message == "3on") digitalWrite(RELAY_HOUSE_3, LOW);
        else if (message == "3off") digitalWrite(RELAY_HOUSE_3, HIGH);
    }
}

void reconnect_mqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32_TheftDetector_" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("connected!");
            mqttClient.subscribe(MQTT_TOPIC_CONTROL);
            Serial.print("Subscribed to: ");
            Serial.println(MQTT_TOPIC_CONTROL);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    // Start Serial for ESP32's own monitor
    Serial.begin(115200);

    // Start Serial2 to listen to the Arduino
    ArduinoSerial.begin(9600, SERIAL_8N1, 16, 17); // RX2=16, TX2=17

    pinMode(RELAY_HOUSE_1, OUTPUT);
    pinMode(RELAY_HOUSE_2, OUTPUT);
    pinMode(RELAY_HOUSE_3, OUTPUT);
    digitalWrite(RELAY_HOUSE_1, HIGH);
    digitalWrite(RELAY_HOUSE_2, HIGH);
    digitalWrite(RELAY_HOUSE_3, HIGH);
    
    setup_wifi();

    // Use setInsecure() for simplified SSL/TLS connection
    wifiClient.setInsecure(); 
    
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
}

void loop() {
    if (!mqttClient.connected()) {
        reconnect_mqtt();
    }
    mqttClient.loop();

    // Check if a complete line of data has arrived from the Arduino
    if (ArduinoSerial.available() > 0) {
        String receivedData = ArduinoSerial.readStringUntil('\n');
        receivedData.trim();

        // --- Parse the Comma-Separated String ---
        // Expected format: "1.85,0.66,1.00,0.66"
        float currents[4];
        int currentSensorIndex = 0;
        String tempValue = "";

        for (int i = 0; i < receivedData.length(); i++) {
            char c = receivedData.charAt(i);
            if (c == ',') {
                if (currentSensorIndex < 4) {
                    currents[currentSensorIndex] = tempValue.toFloat();
                }
                currentSensorIndex++;
                tempValue = "";
            } else {
                tempValue += c;
            }
        }
        // Process the last value in the string
        if (currentSensorIndex < 4) {
             currents[currentSensorIndex] = tempValue.toFloat();
        }

        // --- Process and Publish Data ---
        // Ensure we received exactly 4 values before processing
        if (currentSensorIndex == 3) {
            // Assign values based on Arduino's sending order (A0, A1, A2, A3)
            float house1Current = currents[0]; // From A0 (5A)
            float house2Current = currents[1]; // From A1 (30A)
            float house3Current = currents[2]; // From A2 (20A)
            float poleCurrent   = currents[3]; // From A3 (30A)
            
            float totalHouseCurrent = house1Current + house2Current + house3Current;
            float difference = poleCurrent - totalHouseCurrent;

            // Determine system status
            String status = "Normal";
            if (poleCurrent > 0.3 && difference > THEFT_THRESHOLD) {
                status = "THEFT DETECTED!";
            }

            // Create a JSON object
            StaticJsonDocument<256> doc;
            doc["pole"] = poleCurrent;
            doc["h1"] = house1Current;
            doc["h2"] = house2Current;
            doc["h3"] = house3Current;
            doc["total"] = totalHouseCurrent;
            doc["diff"] = difference;
            doc["status"] = status;

            // Serialize JSON to a string
            char jsonBuffer[256];
            serializeJson(doc, jsonBuffer);

            // Publish the JSON message
            mqttClient.publish(MQTT_TOPIC_DATA, jsonBuffer);
            Serial.print("Received from Arduino and Published to MQTT: ");
            Serial.println(jsonBuffer);
        } else {
            Serial.print("Error: Received incomplete data from Arduino: ");
            Serial.println(receivedData);
        }
    }
}
