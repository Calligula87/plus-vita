/* Host-only SDK boundary declarations; native builds use VitaSDK headers. */
#ifndef TEST_VITA_TOUCH_H
#define TEST_VITA_TOUCH_H
#include <stdint.h>
#define SCE_TOUCH_MAX_REPORT 8
enum { SCE_TOUCH_PORT_FRONT=0, SCE_TOUCH_PORT_BACK=1 };
typedef struct { int16_t minAaX,minAaY,maxAaX,maxAaY; } SceTouchPanelInfo;
typedef struct { uint8_t id; int16_t x,y; } SceTouchReport;
typedef struct { uint64_t timeStamp; uint32_t status,reportNum; SceTouchReport report[8]; } SceTouchData;
int sceTouchPeek(uint32_t port, SceTouchData *touch, uint32_t count);
#endif
