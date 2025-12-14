#ifndef webpage_H
#define webpage_H   


#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>

void initwebservers();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
// called regularly from the main loop to broadcast data
void webBroadcastLoop();

#endif  // webpage_H