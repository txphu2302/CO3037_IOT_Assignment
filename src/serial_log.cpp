#include "serial_log.h"

#include "global.h"

void serialLogInit(SharedContext *ctx)
{
  if (!ctx)
    return;
  if (ctx->mutexSerial == nullptr)
    ctx->mutexSerial = xSemaphoreCreateMutex();
}

void serialLogLock(SharedContext *ctx)
{
  if (!ctx)
    return;
  if (ctx->mutexSerial != nullptr)
    xSemaphoreTake(ctx->mutexSerial, portMAX_DELAY);
}

void serialLogUnlock(SharedContext *ctx)
{
  if (!ctx)
    return;
  if (ctx->mutexSerial != nullptr)
    xSemaphoreGive(ctx->mutexSerial);
}

