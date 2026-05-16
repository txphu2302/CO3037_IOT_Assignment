#ifndef SERIAL_LOG_H
#define SERIAL_LOG_H

struct SharedContext;

/** Call once from setup() after Serial.begin so multi-task Serial output does not interleave. */
void serialLogInit(SharedContext *ctx);
void serialLogLock(SharedContext *ctx);
void serialLogUnlock(SharedContext *ctx);

#endif
