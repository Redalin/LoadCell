#include "webpage.h"
#include "littlefs-conf.h"
#include "scale.h"
#include <ArduinoJson.h>


AsyncWebSocket ws("/ws");
AsyncWebServer server(80);
// broadcast interval (ms)
static const unsigned long BROADCAST_INTERVAL = 1000;
static unsigned long lastBroadcast = 0;


void initwebservers(){ 
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  Serial.println("Starting Web Server");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/favicon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/favicon.png", "image/png");
  });

  // provide a simple HTTP endpoint to tare the scale
  server.on("/tare", HTTP_POST, [](AsyncWebServerRequest *request){
    scaleTareAll();
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();

  Serial.println("Init Done. Ready");
}

// Send current weight to all connected websocket clients as JSON
static void notifyClients(){
  float weight1 = scaleGetUnits1();
  float weight2 = scaleGetUnits2();
  StaticJsonDocument<192> doc;
  if (!isnan(weight1)) doc["weight1"] = weight1; else doc["weight1"] = nullptr;
  if (!isnan(weight2)) doc["weight2"] = weight2; else doc["weight2"] = nullptr;
  char buf[192];
  size_t n = serializeJson(doc, buf);
  ws.textAll(buf, n);
}

void webBroadcastLoop(){
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = now;
    notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // send immediate update when client connects
    notifyClients();
  } else if (type == WS_EVT_DATA) {
    // incoming message (assume simple text commands)
    String msg = String((char*)data).substring(0, len);
    Serial.println("WS Received: " + msg);
    if (msg == "tare") {
      scaleTareAll();
      // send immediate update after tare
      notifyClients();
    }
  }
}