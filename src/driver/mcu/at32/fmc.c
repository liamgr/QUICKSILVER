#include "driver/fmc.h"

#include "core/failloop.h"
#include "core/project.h"

#define FLASH_PTR(offset) (_config_flash + FLASH_ALIGN(offset))

uint8_t __attribute__((section(".config_flash"))) _config_flash[16384];

void fmc_lock() {
  flash_lock();
}

void fmc_unlock() {
  flash_unlock();
}

void fmc_erase() {
  flash_block_erase((uint32_t)_config_flash);
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

  flash_word_t *ptr = (flash_word_t *)data;
  for (uint32_t i = 0; i < (size / FLASH_WORD_SIZE); i++) {
    const uint32_t addr = (uint32_t)FLASH_PTR(start + i * FLASH_WORD_SIZE);
    flash_word_program(addr, ptr[i]);
  }
}