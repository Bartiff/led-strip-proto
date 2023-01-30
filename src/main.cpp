#include <Arduino.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <StreamUtils.h>
#include <FastLED.h>

#include "secret.h"

#define EEPROM_SIZE 100
#define LOGLEVEL LOG_LEVEL_VERBOSE
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#define NUM_LEDS 15
#define DATA_PIN 4
#define COLOR_ORDER GRB
#define LED_TYPE WS2812B

int RGBAcolor[4];
CRGB leds[NUM_LEDS];

WebServer httpServer(80);

// Set Default Leds Values
void setDefaultLedsValues()
{
  int defaultRed = EEPROM.read(0);
  int defaultGreen = EEPROM.read(1);
  int defaultBlue = EEPROM.read(2);
  int defaultAlpha = EEPROM.read(3);
  if (isnan(defaultRed) || isnan(defaultGreen) || isnan(defaultBlue) || isnan(defaultAlpha)) {
    Serial.println("Reload leds values...");
    RGBAcolor[0] = 0;
    RGBAcolor[1] = 0;
    RGBAcolor[2] = 0;
    RGBAcolor[3] = 0;
  } else {
    RGBAcolor[0] = defaultRed;
    RGBAcolor[1] = defaultGreen;
    RGBAcolor[2] = defaultBlue;
    RGBAcolor[3] = defaultAlpha;
  }
}

// Serving Led Strip State
void getState()
{
  DynamicJsonDocument doc(512);
  setDefaultLedsValues();
  doc["red"] = RGBAcolor[0];
  doc["green"] = RGBAcolor[1];
  doc["blue"] = RGBAcolor[2];
  doc["alpha"] = RGBAcolor[3];

  Serial.print(F("Stream..."));
  String buf;
  serializeJson(doc, buf);
  httpServer.send(200, "application/json", buf);
  Serial.print(F("done."));
}
void setState()
{
  String postBody = httpServer.arg("plain");
  Serial.println(postBody);
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, postBody);
  if (error) {
    // if the file didn't open, print an error:
    Serial.print(F("Error parsing JSON "));
    Serial.println(error.c_str());

    String msg = error.c_str();

    httpServer.send(400, F("text/html"), "Error in parsin json body! <br>" + msg);
  } else {
    JsonObject postObj = doc.as<JsonObject>();

    Serial.print(F("HTTP Method: "));
    Serial.println(httpServer.method());

    if (httpServer.method() == HTTP_POST) {
      if (postObj.containsKey("red") && postObj.containsKey("green") && postObj.containsKey("blue") && postObj.containsKey("alpha")) {
        Serial.println(F("done."));

        // Here store data or doing operation
        EEPROM.write(0, postObj[F("red")]);
        EEPROM.write(1, postObj[F("green")]);
        EEPROM.write(2, postObj[F("blue")]);
        EEPROM.write(3, postObj[F("alpha")]);
        EEPROM.commit();
        fill_solid(leds, NUM_LEDS, CRGB(EEPROM.read(0), EEPROM.read(1), EEPROM.read(2)));
        FastLED.setBrightness(EEPROM.read(3));
        FastLED.show();

        // Create the response
        // To get the status of the result you can get the http status so
        // this part can be unusefully
        DynamicJsonDocument doc(512);
        doc["status"] = "ok";

        Serial.print(F("Stream..."));
        String buf;
        serializeJson(doc, buf);

        httpServer.send(201, F("application/json"), buf);
        Serial.print(F("done."));
      } else {
        DynamicJsonDocument doc(512);
        doc["status"] = "ko";
        doc["message"] = F("No data found, or incorrect!");

        Serial.print(F("Stream..."));
        String buf;
        serializeJson(doc, buf);

        httpServer.send(400, F("application/json"), buf);
        Serial.print(F("done."));
      }
    }
  }
}

// Serving WiFi Settings
void getSettings()
{
  DynamicJsonDocument doc(512);

  doc["ip"] = WiFi.localIP().toString();
  doc["gw"] = WiFi.gatewayIP().toString();
  doc["nm"] = WiFi.subnetMask().toString();
  if (httpServer.arg("signalStrength")== "true") {
      doc["signalStrengh"] = WiFi.RSSI();
  }
  if (httpServer.arg("chipInfo")== "true") {
      // doc["chipId"] = ESP.getChipId();
      // doc["flashChipId"] = ESP.getFlashChipId();
      doc["flashChipSize"] = ESP.getFlashChipSize();
      // doc["flashChipRealSize"] = ESP.getFlashChipRealSize();
  }
  if (httpServer.arg("freeHeap")== "true") {
      doc["freeHeap"] = ESP.getFreeHeap();
  }

  Serial.print(F("Stream..."));
  String buf;
  serializeJson(doc, buf);
  httpServer.send(200, F("application/json"), buf);
  Serial.print(F("done."));
}

// Define routing
void restServerRouting()
{
  httpServer.on("/", HTTP_GET, []() {
      httpServer.send(200, F("text/html"), F("Ok"));
  });
  httpServer.on(F("/state"), HTTP_GET, getState);
  httpServer.on(F("/settings"), HTTP_GET, getSettings);
  httpServer.on(F("/change"), HTTP_POST, setState);
}

// Manage not found URL
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
}

void setup()
{
  // Setup serial port
  Serial.begin(115200);
  while (!Serial) continue;

  // Init EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Start RGB Leds
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  setDefaultLedsValues();
  fill_solid(leds, NUM_LEDS, CRGB(RGBAcolor[0], RGBAcolor[1], RGBAcolor[2]));
  FastLED.setBrightness(RGBAcolor[3]);
  FastLED.show();

  // WiFi Connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set server routing
  restServerRouting();
  // Set not found response
  httpServer.onNotFound(handleNotFound);
  // Start server
  httpServer.begin();
  Serial.println("HTTP server started");
}

void loop()
{	
  httpServer.handleClient();
}