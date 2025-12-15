#include "webpage.h"
#include "littlefs-conf.h"
#include "scale.h"
#include "settings.h"
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

  // settings endpoints
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String s = settingsAsJson();
    request->send(200, "application/json", s);
  });
  server.on("/settings/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    settingsResetDefaults();
    String s = settingsAsJson();
    request->send(200, "application/json", s);
  });
  // POST /settings with JSON body
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    // ack will be sent from body handler
  }, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    // collect body (may be called multiple times)
    String body;
    body.reserve(total);
    body = String((const char*)data, len);
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { request->send(400, "application/json", "{\"error\":\"invalid json\"}"); return; }
    if (doc.containsKey("name1")) settingsSetName(1, String((const char*)doc["name1"]));
    if (doc.containsKey("name2")) settingsSetName(2, String((const char*)doc["name2"]));
    if (doc.containsKey("color1")) settingsSetColor(1, String((const char*)doc["color1"]));
    if (doc.containsKey("color2")) settingsSetColor(2, String((const char*)doc["color2"]));
    bool ok = settingsSave();
    if (ok) request->send(200, "application/json", "{\"status\":\"ok\"}"); else request->send(500, "application/json", "{\"error\":\"save failed\"}");
  });

  // provide a simple HTTP endpoint to tare the scale
  server.on("/tare", HTTP_POST, [](AsyncWebServerRequest *request){
    int which = 0;
    if (request->hasArg("scale")) {
      which = request->arg("scale").toInt();
    }
    if (which == 1) scaleTare(1);
    else if (which == 2) scaleTare(2);
    else scaleTareAll();
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
    // commands: tare, tare:1, tare:2, calibrate:1, calibrate:2
    if (msg == "tare") {
      scaleTareAll();
      notifyClients();
    } else if (msg.startsWith("tare:")) {
      int which = msg.substring(msg.indexOf(':') + 1).toInt();
      scaleTare(which);
      notifyClients();
    } else if (msg.startsWith("calibrate:")) {
      int which = msg.substring(msg.indexOf(':') + 1).toInt();
      // start async calibration; pass client id so result can be returned
      uint32_t cid = client ? client->id() : 0;
      scaleCalibrateAsync(which, cid);
      // immediate ack to requester
      if (client) client->text("{\"status\":\"calibrating\"}");
    }
  }
}

// send calibration result to clients (declared in header)
void sendCalibrationResult(int which, float value, uint32_t clientId) {
  StaticJsonDocument<128> doc;
  if (!isnan(value)) doc["calibrate"] = value; else doc["calibrate"] = nullptr;
  doc["scale"] = which;
  char buf[128]; size_t n = serializeJson(doc, buf);
  // if clientId provided, send to that client only when available
  if (clientId != 0) {
    // try to find client and send; fall back to broadcast
    AsyncWebSocketClient *c = ws.client(clientId);
    if (c) { c->text(buf, n); return; }
  }
  ws.textAll(buf, n);
}