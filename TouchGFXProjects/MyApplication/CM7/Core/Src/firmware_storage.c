#include "firmware_storage.h"
#include "stm32h7xx_hal.h"

void FirmwareStorage_SaveState(uint32_t state)
{
    // 1. Sauvegarde dans Flash non-volatile (Secteur 7 de la Banque 1, à 0x080E0000)
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Banks        = FLASH_BANK_1;
    EraseInitStruct.Sector       = FLASH_SECTOR_7;
    EraseInitStruct.NbSectors    = 1;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t SectorError = 0;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK)
    {
        // Mot Flash STM32H7 de 256 bits (32 octets = 8 x uint32_t)
        uint32_t flashWord[8] = { state, 0, 0, 0, 0, 0, 0, 0 };
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, FIRMWARE_STATE_FLASH_ADDR, (uint32_t)flashWord);
    }

    HAL_FLASH_Lock();

    // 2. Sauvegarde simultanée dans RTC Backup Register (Domaine Backup)
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = state;
    *(volatile uint32_t*)0x38000400UL = state;
}

uint32_t FirmwareStorage_ReadState(void)
{
    // 1. Lecture directe depuis la mémoire Flash non-volatile
    uint32_t flashVal = (*(volatile uint32_t*)FIRMWARE_STATE_FLASH_ADDR);
    if (flashVal == FIRMWARE_STATE_MAGIC_V2)
    {
        return FIRMWARE_STATE_MAGIC_V2;
    }

    // 2. Vérification de secours dans le registre RTC Backup
    HAL_PWR_EnableBkUpAccess();
    if (RTC->BKP0R == FIRMWARE_STATE_MAGIC_V2)
    {
        return FIRMWARE_STATE_MAGIC_V2;
    }

    // 3. Vérification SRAM4
    if (*(volatile uint32_t*)0x38000400UL == FIRMWARE_STATE_MAGIC_V2)
    {
        return FIRMWARE_STATE_MAGIC_V2;
    }

    return 0; // Mode normal v1.0.0
}

bool FirmwareStorage_IsV2Updated(void)
{
    return (FirmwareStorage_ReadState() == FIRMWARE_STATE_MAGIC_V2);
}
