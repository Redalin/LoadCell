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
    scaleTare();
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();

  Serial.println("Init Done. Ready");
}

// Send current weight to all connected websocket clients as JSON
static void notifyClients(){
  float weight = scaleGetUnits();
  StaticJsonDocument<128> doc;
  if (!isnan(weight)) {
    doc["weight"] = weight;
  } else {
    doc["weight"] = nullptr;
  }
  char buf[128];
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
      scaleTare();
      // send immediate update after tare
      notifyClients();
    }
  }
}