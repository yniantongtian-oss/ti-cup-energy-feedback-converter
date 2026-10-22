#ifndef APP_CONVERTER_H
#define APP_CONVERTER_H

#include "converter_runtime.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void AppConverter_Init(void);
void AppConverter_1msTask(uint32_t now_ms);
void AppConverter_SetCurrentMilliamp(int16_t current_ma, uint32_t now_ms);
bool AppConverter_Arm(uint32_t now_ms);
void AppConverter_Disarm(void);
bool AppConverter_ClearFaults(void);
void AppConverter_EmergencyStop(void);

/* Local human reset. Refuses while the emergency-stop input is still asserted.
 * A remote ClearFault must not call this.
 */
bool AppConverter_AcknowledgeEstop(void);

const converter_runtime_t *AppConverter_GetRuntime(void);
const volatile uint16_t *AppConverter_GetAdcDmaBuffer(void);

#ifdef __cplusplus
}
#endif

#endif
