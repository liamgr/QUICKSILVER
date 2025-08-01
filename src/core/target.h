#pragma once

#include <cbor.h>
#include <stdbool.h>

#include "driver/dma.h"
#include "rx/rx.h"

#define LED_MAX 4

#define GPIO_AF(pin, af, tag)
#define GPIO_PIN(port, num) PIN_##port##num,
typedef enum {
  PIN_NONE,
#include "gpio_pins.in"
  PINS_MAX,
} __attribute__((__packed__)) gpio_pins_t;
#undef GPIO_PIN
#undef GPIO_AF

typedef enum {
  MOTOR_PIN0,
  MOTOR_PIN1,
  MOTOR_PIN2,
  MOTOR_PIN3,
  MOTOR_PIN_MAX
} __attribute__((__packed__)) motor_pin_t;

typedef enum {
  SPI_PORT_INVALID,
  SPI_PORT1,
  SPI_PORT2,
  SPI_PORT3,
  SPI_PORT4,
  SPI_PORT5,
  SPI_PORT6,
  SPI_PORT_MAX,
} __attribute__((__packed__)) spi_ports_t;

typedef enum {
  SERIAL_PORT_INVALID,
  SERIAL_PORT1,
  SERIAL_PORT2,
#if !defined(STM32F411)
  SERIAL_PORT3,
  SERIAL_PORT4,
  SERIAL_PORT5,
#endif
#ifndef STM32G4
  SERIAL_PORT6,
#endif
#if defined(STM32F7) || defined(STM32H7) || defined(AT32F4)
  SERIAL_PORT7,
  SERIAL_PORT8,
#endif
  SERIAL_PORT_MAX,

  SERIAL_SOFT_INVALID = 100,
  SERIAL_SOFT_PORT1,
  SERIAL_SOFT_PORT2,
  SERIAL_SOFT_PORT3,
  SERIAL_SOFT_MAX,

  SERIAL_SOFT_START = SERIAL_SOFT_INVALID,
  SERIAL_SOFT_COUNT = SERIAL_SOFT_MAX - SERIAL_SOFT_START,
} __attribute__((__packed__)) serial_ports_t;

typedef struct {
#if defined(STM32H7) || defined(STM32G4) || defined(AT32)
  uint32_t request;
#else
  uint32_t channel;
#endif
  resource_tag_t tag;
  dma_stream_t dma;
} target_dma_t;

#if defined(STM32H7) || defined(STM32G4) || defined(AT32)
#define TARGET_DMA_MEMBERS    \
  START_STRUCT(target_dma_t)  \
  MEMBER(request, uint32_t)   \
  MEMBER(tag, resource_tag_t) \
  MEMBER(dma, dma_stream_t)   \
  END_STRUCT()
#else
#define TARGET_DMA_MEMBERS    \
  START_STRUCT(target_dma_t)  \
  MEMBER(channel, uint32_t)   \
  MEMBER(tag, resource_tag_t) \
  MEMBER(dma, dma_stream_t)   \
  END_STRUCT()
#endif

typedef struct {
  gpio_pins_t pin;
  bool invert;
} target_led_t;

#define TARGET_LED_MEMBERS   \
  START_STRUCT(target_led_t) \
  MEMBER(pin, gpio_pins_t)   \
  MEMBER(invert, bool)       \
  END_STRUCT()

typedef struct {
  gpio_pins_t pin;
  bool invert;
} target_invert_pin_t;

#define TARGET_BUZZER_MEMBERS       \
  START_STRUCT(target_invert_pin_t) \
  MEMBER(pin, gpio_pins_t)          \
  MEMBER(invert, bool)              \
  END_STRUCT()

typedef struct {
  uint8_t index;
  gpio_pins_t rx;
  gpio_pins_t tx;
  gpio_pins_t inverter;
} target_serial_port_t;

#define TARGET_SERIAL_MEMBERS        \
  START_STRUCT(target_serial_port_t) \
  MEMBER(index, uint8_t)             \
  MEMBER(rx, gpio_pins_t)            \
  MEMBER(tx, gpio_pins_t)            \
  MEMBER(inverter, gpio_pins_t)      \
  END_STRUCT()

typedef struct {
  uint8_t index;
  gpio_pins_t miso;
  gpio_pins_t mosi;
  gpio_pins_t sck;
} target_spi_port_t;

#define TARGET_SPI_MEMBERS        \
  START_STRUCT(target_spi_port_t) \
  MEMBER(index, uint8_t)          \
  MEMBER(miso, gpio_pins_t)       \
  MEMBER(mosi, gpio_pins_t)       \
  MEMBER(sck, gpio_pins_t)        \
  END_STRUCT()

typedef struct {
  spi_ports_t port;
  gpio_pins_t nss;
} target_spi_device_t;

#define TARGET_SPI_DEVICE_MEMBERS   \
  START_STRUCT(target_spi_device_t) \
  MEMBER(port, uint8_t)             \
  MEMBER(nss, gpio_pins_t)          \
  END_STRUCT()

typedef struct {
  spi_ports_t port;
  gpio_pins_t nss;
  gpio_pins_t exti;
} target_gyro_spi_device_t;

#define TARGET_GYRO_SPI_DEVICE_MEMBERS   \
  START_STRUCT(target_gyro_spi_device_t) \
  MEMBER(port, uint8_t)                  \
  MEMBER(nss, gpio_pins_t)               \
  MEMBER(exti, gpio_pins_t)              \
  END_STRUCT()

typedef struct {
  spi_ports_t port;
  gpio_pins_t nss;
  gpio_pins_t exti;
  gpio_pins_t ant_sel;
  gpio_pins_t lna_en;
  gpio_pins_t tx_en;
  gpio_pins_t busy;
  bool busy_exti;
  gpio_pins_t reset;
} target_rx_spi_device_t;

#define TARGET_RX_SPI_DEVICE_MEMBERS   \
  START_STRUCT(target_rx_spi_device_t) \
  MEMBER(port, uint8_t)                \
  MEMBER(nss, gpio_pins_t)             \
  MEMBER(exti, gpio_pins_t)            \
  MEMBER(ant_sel, gpio_pins_t)         \
  MEMBER(lna_en, gpio_pins_t)          \
  MEMBER(tx_en, gpio_pins_t)           \
  MEMBER(busy, gpio_pins_t)            \
  MEMBER(busy_exti, bool)              \
  MEMBER(reset, gpio_pins_t)           \
  END_STRUCT()

typedef struct {
  uint8_t name[32];

  bool brushless;

  target_led_t leds[LED_MAX];
  target_serial_port_t serial_ports[SERIAL_PORT_MAX];
  target_serial_port_t serial_soft_ports[SERIAL_SOFT_COUNT];
  target_spi_port_t spi_ports[SPI_PORT_MAX];

  target_gyro_spi_device_t gyro;
  uint8_t gyro_orientation;
  target_spi_device_t osd;
  target_spi_device_t flash;
  target_spi_device_t sdcard;
  target_rx_spi_device_t rx_spi;

  gpio_pins_t usb_detect;
  gpio_pins_t fpv;
  gpio_pins_t vbat;
  gpio_pins_t ibat;
  gpio_pins_t rgb_led;

  target_invert_pin_t sdcard_detect;
  target_invert_pin_t buzzer;
  gpio_pins_t motor_pins[MOTOR_PIN_MAX];

  uint16_t vbat_scale;
  uint16_t ibat_scale;

  target_dma_t dma[DMA_DEVICE_MAX];
} target_t;

#define TARGET_MEMBERS                                                           \
  START_STRUCT(target_t)                                                         \
  TSTR_MEMBER(name, 32)                                                          \
  MEMBER(brushless, bool)                                                        \
  ARRAY_MEMBER(leds, LED_MAX, target_led_t)                                      \
  INDEX_ARRAY_MEMBER(serial_ports, SERIAL_PORT_MAX, target_serial_port_t)        \
  INDEX_ARRAY_MEMBER(serial_soft_ports, SERIAL_SOFT_COUNT, target_serial_port_t) \
  INDEX_ARRAY_MEMBER(spi_ports, SPI_PORT_MAX, target_spi_port_t)                 \
  MEMBER(gyro, target_gyro_spi_device_t)                                         \
  MEMBER(gyro_orientation, uint8_t)                                              \
  MEMBER(osd, target_spi_device_t)                                               \
  MEMBER(flash, target_spi_device_t)                                             \
  MEMBER(sdcard, target_spi_device_t)                                            \
  MEMBER(rx_spi, target_rx_spi_device_t)                                         \
  MEMBER(usb_detect, gpio_pins_t)                                                \
  MEMBER(fpv, gpio_pins_t)                                                       \
  MEMBER(vbat, gpio_pins_t)                                                      \
  MEMBER(ibat, gpio_pins_t)                                                      \
  MEMBER(rgb_led, gpio_pins_t)                                                   \
  MEMBER(sdcard_detect, target_invert_pin_t)                                     \
  MEMBER(buzzer, target_invert_pin_t)                                            \
  ARRAY_MEMBER(motor_pins, MOTOR_PIN_MAX, gpio_pins_t)                           \
  MEMBER(vbat_scale, uint16_t)                                                   \
  MEMBER(ibat_scale, uint16_t)                                                   \
  MEMBER(dma, target_dma_array)                                                  \
  END_STRUCT()

typedef enum {
  FEATURE_BRUSHLESS = (1 << 1),
  FEATURE_OSD = (1 << 2),
  FEATURE_BLACKBOX = (1 << 3),
  FEATURE_DEBUG = (1 << 4),
} target_feature_t;

typedef struct {
  const char *mcu;
  const char *git_version;
  uint32_t quic_protocol_version;

  uint32_t features;
  rx_protocol_t rx_protocols[RX_PROTOCOL_MAX];

  uint8_t gyro_id;
} target_info_t;

#define TARGET_INFO_MEMBERS                            \
  START_STRUCT(target_info_t)                          \
  STR_MEMBER(mcu)                                      \
  STR_MEMBER(git_version)                              \
  MEMBER(quic_protocol_version, uint32_t)              \
  MEMBER(features, uint32_t)                           \
  ARRAY_MEMBER(rx_protocols, RX_PROTOCOL_MAX, uint8_t) \
  MEMBER(gyro_id, uint8_t)                             \
  END_STRUCT()

extern target_t target;
extern target_info_t target_info;

void target_set_feature(target_feature_t feat);
void target_reset_feature(target_feature_t feat);

void target_add_rx_protocol(rx_protocol_t proto);
bool target_has_rx_protocol(rx_protocol_t proto);

bool target_serial_port_valid(const target_serial_port_t *port);
bool target_gyro_spi_device_valid(const target_gyro_spi_device_t *dev);
bool target_spi_device_valid(const target_spi_device_t *dev);
bool target_spi_port_valid(const target_spi_port_t *port);

cbor_result_t cbor_encode_gpio_pins_t(cbor_value_t *enc, const gpio_pins_t *t);
cbor_result_t cbor_decode_gpio_pins_t(cbor_value_t *dec, gpio_pins_t *t);

cbor_result_t cbor_encode_target_t(cbor_value_t *enc, const target_t *t);
cbor_result_t cbor_decode_target_t(cbor_value_t *dec, target_t *t);

cbor_result_t cbor_encode_target_dma_t(cbor_value_t *enc, const target_dma_t *dma);
cbor_result_t cbor_decode_target_dma_t(cbor_value_t *dec, target_dma_t *dma);

cbor_result_t cbor_encode_target_info_t(cbor_value_t *enc, const target_info_t *i);

cbor_result_t cbor_encode_target_dma_array(cbor_value_t *dec, const target_dma_t (*dma)[DMA_DEVICE_MAX]);
cbor_result_t cbor_decode_target_dma_array(cbor_value_t *dec, target_dma_t (*dma)[DMA_DEVICE_MAX]);
