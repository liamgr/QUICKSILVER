#include "driver/serial_4way_avr_bl.h"

#include "driver/gpio.h"
#include "driver/serial_4way.h"
#include "driver/serial_esc.h"
#include "driver/time.h"

#ifdef USE_MOTOR_DSHOT

#define BOOT_MSG_LEN 4
#define DevSignHi (BOOT_MSG_LEN)
#define DevSignLo (BOOT_MSG_LEN + 1)

// Bootloader commands
// RunCmd
#define RestartBootloader 0
#define ExitBootloader 1

#define CMD_RUN 0x00
#define CMD_PROG_FLASH 0x01
#define CMD_ERASE_FLASH 0x02
#define CMD_READ_FLASH_SIL 0x03
#define CMD_VERIFY_FLASH 0x03
#define CMD_VERIFY_FLASH_ARM 0x04
#define CMD_READ_EEPROM 0x04
#define CMD_PROG_EEPROM 0x05
#define CMD_READ_SRAM 0x06
#define CMD_READ_FLASH_ATM 0x07
#define CMD_KEEP_ALIVE 0xFD
#define CMD_SET_ADDRESS 0xFF
#define CMD_SET_BUFFER 0xFE

#define CMD_BOOTINIT 0x07
#define CMD_BOOTSIGN 0x08

#define ESC_BAUD 19200
#define START_BIT_TIMEOUT_MS 2

extern bool device_is_connected();

static uint16_t crc_update(uint8_t data, uint16_t crc) {
  for (uint8_t i = 0; i < 8; i++) {
    if (((data & 0x01) ^ (crc & 0x0001)) != 0) {
      crc = crc >> 1;
      crc = crc ^ 0xA001;
    } else {
      crc = crc >> 1;
    }
    data = data >> 1;
  }
  return crc;
}

static uint8_t avr_bl_read(gpio_pins_t esc, uint8_t *buf, uint8_t size) {
  uint16_t crc = 0;

  // len 0 means 256
  for (uint16_t i = 0; i < (size == 0 ? 256 : size); i++) {
    if (!serial_esc_read(esc, ESC_BAUD, buf + i)) {
      return 0;
    }
    crc = crc_update(buf[i], crc);
  }

  uint8_t ack = brNONE;
  if (device_is_connected()) {
    // With CRC read 3 more
    uint8_t crc_buf[2];
    if (!serial_esc_read(esc, ESC_BAUD, &crc_buf[0])) {
      return 0;
    }
    if (!serial_esc_read(esc, ESC_BAUD, &crc_buf[1])) {
      return 0;
    }

    uint16_t their_crc = (uint16_t)(crc_buf[1] << 8) | crc_buf[0];
    if (their_crc != crc) {
      return 0;
    }
  }

  if (!serial_esc_read(esc, ESC_BAUD, &ack)) {
    return 0;
  }
  return ack == brSUCCESS;
}

static void avr_bl_write(gpio_pins_t esc, const uint8_t *buf, const uint8_t size) {
  serial_esc_set_output(esc);

  uint16_t crc = 0;
  for (uint16_t i = 0; i < (size == 0 ? 256 : size); i++) {
    serial_esc_write(esc, ESC_BAUD, buf[i]);
    crc = crc_update(buf[i], crc);
  }

  if (device_is_connected()) {
    serial_esc_write(esc, ESC_BAUD, (crc >> 0) & 0xFF);
    serial_esc_write(esc, ESC_BAUD, (crc >> 8) & 0xFF);
  }

  serial_esc_set_input(esc);
}

static uint8_t avr_bl_get_ack(gpio_pins_t pin, uint32_t timeout) {
  uint8_t ack = brNONE;
  while (!serial_esc_read(pin, ESC_BAUD, &ack) && timeout) {
    timeout--;
  };
  return ack;
}

static uint8_t avr_bl_send_set_addr(gpio_pins_t pin, uint16_t addr) {
  if (addr == 0xFFFF) {
    return 1;
  }

  const uint8_t buf[4] = {
      CMD_SET_ADDRESS,
      0,
      (addr >> 8) & 0xFF,
      addr & 0xFF,
  };
  avr_bl_write(pin, buf, 4);

  return avr_bl_get_ack(pin, 2) == brSUCCESS;
}

static uint8_t avr_bl_send_set_buffer(gpio_pins_t pin, const uint8_t *data, uint8_t size) {
  uint8_t buf[] = {
      CMD_SET_BUFFER,
      0,
      0,
      size,
  };
  if (size == 0) {
    // set high byte
    buf[2] = 1;
  }
  avr_bl_write(pin, buf, 4);

  if (avr_bl_get_ack(pin, 2) != brNONE)
    return 0;

  avr_bl_write(pin, data, size);

  return avr_bl_get_ack(pin, 40) == brSUCCESS;
}

void avr_bl_init_pin(gpio_pins_t pin) {
  serial_esc_set_input(pin);
  serial_esc_set_high(pin);
}

uint8_t avr_bl_connect(gpio_pins_t pin, uint8_t *data) {
  uint8_t boot_msg[BOOT_MSG_LEN - 1] = "471";
  uint8_t boot_init[] = {0, 0, 0, 0, 0, 0, 0, 0, 0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
  avr_bl_write(pin, boot_init, 17);

  uint8_t boot_info[9];
  if (!avr_bl_read(pin, boot_info, BOOT_MSG_LEN + 4)) {
    return 0;
  }

  // boot_info has no CRC  (ACK byte already analyzed... )
  // Format = boot_msg("471c") SIGNATURE_001, SIGNATURE_002, BootVersion (always 6), BootPages (,ACK)
  for (uint8_t i = 0; i < (BOOT_MSG_LEN - 1); i++) { // Check only the first 3 letters -> 471x OK
    if (boot_info[i] != boot_msg[i]) {
      return 0;
    }
  }

  // only 2 bytes used $1E9307 -> 0x9307
  data[2] = boot_info[BOOT_MSG_LEN - 1];
  data[1] = boot_info[DevSignHi];
  data[0] = boot_info[DevSignLo];
  return 1;
}

uint8_t avr_bl_send_keepalive(gpio_pins_t pin) {
  uint8_t buf[] = {CMD_KEEP_ALIVE, 0};
  avr_bl_write(pin, buf, 2);

  if (avr_bl_get_ack(pin, 1) != brERRORCOMMAND) {
    return 0;
  }

  return 1;
}

void avr_bl_send_restart(gpio_pins_t pin, uint8_t *data) {
  serial_esc_set_output(pin);
  serial_esc_write(pin, ESC_BAUD, RestartBootloader);
  serial_esc_write(pin, ESC_BAUD, 0);
  serial_esc_write(pin, ESC_BAUD, 0);
  serial_esc_write(pin, ESC_BAUD, 0);
  serial_esc_set_input(pin);
  data[0] = 1;
}

void avr_bl_reboot(gpio_pins_t pin) {
  serial_esc_set_output(pin);

  serial_esc_set_low(pin);
  time_delay_ms(300);
  serial_esc_set_high(pin);

  serial_esc_set_input(pin);
}

static uint8_t avr_bl_read_cmd(gpio_pins_t pin, uint8_t cmd, uint16_t addr, uint8_t *data, uint8_t size) {
  if (!avr_bl_send_set_addr(pin, addr)) {
    return 0;
  }

  uint8_t buf[] = {cmd, size};
  avr_bl_write(pin, buf, 2);

  return avr_bl_read(pin, data, size);
}

static uint8_t avr_bl_write_cmd(gpio_pins_t pin, uint8_t cmd, uint16_t addr, const uint8_t *data, uint8_t size, uint32_t timeout) {
  if (!avr_bl_send_set_addr(pin, addr)) {
    return 0;
  }

  if (!avr_bl_send_set_buffer(pin, data, size)) {
    return 0;
  }

  uint8_t buf[] = {cmd, 0x01};
  avr_bl_write(pin, buf, 2);

  return avr_bl_get_ack(pin, timeout) == brSUCCESS;
}

uint8_t avr_bl_read_flash(gpio_pins_t pin, uint8_t interface_mode, uint16_t addr, uint8_t *data, uint8_t size) {
  if (interface_mode == ESC4WAY_ATM_BLB) {
    return avr_bl_read_cmd(pin, CMD_READ_FLASH_ATM, addr, data, size);
  } else {
    return avr_bl_read_cmd(pin, CMD_READ_FLASH_SIL, addr, data, size);
  }
}

uint8_t avr_bl_read_eeprom(gpio_pins_t pin, uint16_t addr, uint8_t *data, uint8_t size) {
  return avr_bl_read_cmd(pin, CMD_READ_EEPROM, addr, data, size);
}

uint8_t avr_bl_page_erase(gpio_pins_t pin, uint16_t addr) {
  if (!avr_bl_send_set_addr(pin, addr)) {
    return 0;
  }

  uint8_t buf[] = {CMD_ERASE_FLASH, 0x01};
  avr_bl_write(pin, buf, 2);

  return avr_bl_get_ack(pin, (3000 / START_BIT_TIMEOUT_MS)) == brSUCCESS;
}

uint8_t avr_bl_write_eeprom(gpio_pins_t pin, uint16_t addr, const uint8_t *data, uint8_t size) {
  return avr_bl_write_cmd(pin, CMD_PROG_EEPROM, addr, data, size, (3000 / START_BIT_TIMEOUT_MS));
}

uint8_t avr_bl_write_flash(gpio_pins_t pin, uint16_t addr, const uint8_t *data, uint8_t size) {
  return avr_bl_write_cmd(pin, CMD_PROG_FLASH, addr, data, size, (500 / START_BIT_TIMEOUT_MS));
}

uint8_t avr_bl_verify_flash(gpio_pins_t pin, uint16_t addr, const uint8_t *data, uint8_t size) {
  if (!avr_bl_send_set_addr(pin, addr)) {
    return 0;
  }

  if (!avr_bl_send_set_buffer(pin, data, size)) {
    return 0;
  }

  uint8_t buf[] = {CMD_VERIFY_FLASH_ARM, 0x01};
  avr_bl_write(pin, buf, 2);

  return avr_bl_get_ack(pin, 40 / START_BIT_TIMEOUT_MS);
}

#endif