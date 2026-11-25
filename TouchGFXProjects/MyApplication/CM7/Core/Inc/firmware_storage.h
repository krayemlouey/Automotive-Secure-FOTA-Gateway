#ifndef FIRMWARE_STORAGE_H
#define FIRMWARE_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_STATE_MAGIC_V2   0x55AA2000UL
#define FIRMWARE_STATE_FLASH_ADDR 0x080E0000UL

void FirmwareStorage_SaveState(uint32_t state);
uint32_t FirmwareStorage_ReadState(void);
bool FirmwareStorage_IsV2Updated(void);

#ifdef __cplusplus
}
#endif

#endif // FIRMWARE_STORAGE_H
