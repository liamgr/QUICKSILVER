#include "driver/fmc.h"

#include <string.h>

#include "core/failloop.h"
#include "core/project.h"

#if defined(STM32F4)
#define FLASH_FLAG_ALL_ERRORS (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)
#endif

#if defined(STM32H7)
#define FLASH_FLAG_ALL_ERRORS (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGSERR)
#endif

#if defined(STM32G4)
#define PROGRAM_TYPE FLASH_TYPEPROGRAM_DOUBLEWORD
#elif defined(STM32H7)
#define PROGRAM_TYPE FLASH_TYPEPROGRAM_FLASHWORD
#else
#define PROGRAM_TYPE FLASH_TYPEPROGRAM_WORD
#endif

#define FLASH_PTR(offset) (_config_flash + FLASH_ALIGN(offset))

uint8_t __attribute__((section(".config_flash"))) _config_flash[16384];

void fmc_lock() {
  HAL_FLASH_Lock();
}

void fmc_unlock() {
  HAL_FLASH_Unlock();
}

void fmc_erase() {
  // clear error status
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
#if defined(STM32G4)
  uint32_t page_error;
  FLASH_EraseInitTypeDef erase_init;
  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init.Banks = FLASH_BANK_1;
  erase_init.Page = 24;
  erase_init.NbPages = 8;
  HAL_FLASHEx_Erase(&erase_init, &page_error);
#elif defined(STM32H7)
  FLASH_Erase_Sector(FLASH_SECTOR_1, FLASH_BANK_BOTH, FLASH_VOLTAGE_RANGE_3);
#else
  FLASH_Erase_Sector(FLASH_SECTOR_3, FLASH_VOLTAGE_RANGE_3);
#endif
}

flash_word_t fmc_read(uint32_t addr) {
  return *((flash_word_t *)(_config_flash + addr));
}

void fmc_read_buf(uint32_t start, uint8_t *data, uint32_t size) {
  if (size < FLASH_WORD_SIZE || (size % FLASH_WORD_SIZE) || (start + size) > sizeof(_config_flash)) {
    fmc_lock();
    failloop(FAILLOOP_FAULT);
  }

  flash_word_t *ptr = (flash_word_t *)data;
  for (uint32_t i = 0; i < (size / sizeof(flash_word_t)); i++) {
    ptr[i] = fmc_read(start + i * sizeof(flash_word_t));
  }
}

void fmc_write_buf(uint32_t start, uint8_t *data, uint32_t size) {
  if (size < FLASH_WORD_SIZE || (size % FLASH_WORD_SIZE) || (start + size) > sizeof(_config_flash)) {
    fmc_lock();
    failloop(FAILLOOP_FAULT);
  }

  for (uint32_t i = 0; i < (size / FLASH_WORD_SIZE); i++) {
    const uint32_t addr = (uint32_t)FLASH_PTR(start + i * FLASH_WORD_SIZE);

#ifdef STM32H7
    const uint32_t ptr = (uint32_t)(data + i * FLASH_WORD_SIZE);
    HAL_StatusTypeDef result = HAL_FLASH_Program(PROGRAM_TYPE, addr, ptr);
#else
    flash_word_t *ptr = (flash_word_t *)data;
    HAL_StatusTypeDef result = HAL_FLASH_Program(PROGRAM_TYPE, addr, ptr[i]);
#endif

    if (result != HAL_OK) {
      fmc_lock();
      failloop(FAILLOOP_FAULT);
    }
  }
}
