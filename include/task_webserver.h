
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <task_handler.h>
#include "global.h"

void Webserver_stop(SharedContext *ctx);
void Webserver_reconnect(SharedContext *ctx);
void Webserver_sendata(SharedContext *ctx, const String &data);

#endif
