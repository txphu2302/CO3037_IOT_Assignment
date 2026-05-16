
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include "global.h"

extern void handleWebSocketMessage(SharedContext *ctx, const String &message);
#endif
