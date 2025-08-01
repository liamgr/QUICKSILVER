#include "driver/spi.h"

#include "core/failloop.h"
#include "driver/interrupt.h"

extern bool spi_txn_can_send(spi_bus_device_t *bus, bool dma);
extern void spi_txn_finish(spi_ports_t port);

const spi_port_def_t spi_port_defs[SPI_PORT_MAX] = {
    {},
    {
        .channel_index = 1,
        .channel = SPI1,
        .rcc = RCC_ENCODE(SPI1),
        .dma_rx = DMA_DEVICE_SPI1_RX,
        .dma_tx = DMA_DEVICE_SPI1_TX,
    },
    {
        .channel_index = 2,
        .channel = SPI2,
        .rcc = RCC_ENCODE(SPI2),
        .dma_rx = DMA_DEVICE_SPI2_RX,
        .dma_tx = DMA_DEVICE_SPI2_TX,
    },
    {
        .channel_index = 3,
        .channel = SPI3,
        .rcc = RCC_ENCODE(SPI3),
        .dma_rx = DMA_DEVICE_SPI3_RX,
        .dma_tx = DMA_DEVICE_SPI3_TX,
    },
    {
        .channel_index = 4,
        .channel = SPI4,
        .rcc = RCC_ENCODE(SPI4),
        .dma_rx = DMA_DEVICE_SPI4_RX,
        .dma_tx = DMA_DEVICE_SPI4_TX,
    },
};

extern FAST_RAM spi_txn_t txn_pool[SPI_TXN_MAX];

static uint32_t spi_divider_to_ll(uint32_t divider) {
  switch (divider) {
  default:
  case 2:
    return SPI_MCLK_DIV_2;
  case 4:
    return SPI_MCLK_DIV_4;
  case 8:
    return SPI_MCLK_DIV_8;
  case 16:
    return SPI_MCLK_DIV_16;
  case 32:
    return SPI_MCLK_DIV_32;
  case 64:
    return SPI_MCLK_DIV_64;
  case 128:
    return SPI_MCLK_DIV_128;
  case 256:
    return SPI_MCLK_DIV_256;
  case 512:
    return SPI_MCLK_DIV_512;
  case 1024:
    return SPI_MCLK_DIV_1024;
  }
}

static uint32_t spi_find_divder(uint32_t clk_hz) {
  uint32_t divider = 2;
  uint32_t clock = SPI_CLOCK_FREQ_HZ / divider;

  while ((clock > clk_hz) && (divider < 1024)) {
    divider = divider << 1;
    clock = clock >> 1;
  }

  return spi_divider_to_ll(divider);
}

static void spi_dma_init_rx(spi_ports_t port) {
  const dma_stream_def_t *dma = &dma_stream_defs[target.dma[spi_port_defs[port].dma_rx].dma];

  dma_reset(dma->stream);
  dmamux_init(dma->mux, (dmamux_requst_id_sel_type)target.dma[spi_port_defs[port].dma_rx].request);

  dma_init_type init;
  init.peripheral_base_addr = (uint32_t)(&spi_port_defs[port].channel->dt);
  init.memory_base_addr = 0;
  init.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
  init.buffer_size = 0;
  init.peripheral_inc_enable = FALSE;
  init.memory_inc_enable = TRUE;
  init.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
  init.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
  init.loop_mode_enable = FALSE;
  init.priority = DMA_PRIORITY_HIGH;
  dma_init(dma->stream, &init);
}

static void spi_dma_init_tx(spi_ports_t port) {
  const dma_stream_def_t *dma = &dma_stream_defs[target.dma[spi_port_defs[port].dma_tx].dma];

  dma_reset(dma->stream);
  dmamux_init(dma->mux, (dmamux_requst_id_sel_type)target.dma[spi_port_defs[port].dma_tx].request);

  dma_init_type init;
  init.peripheral_base_addr = (uint32_t)(&spi_port_defs[port].channel->dt);
  init.memory_base_addr = 0;
  init.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
  init.buffer_size = 0;
  init.peripheral_inc_enable = FALSE;
  init.memory_inc_enable = TRUE;
  init.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
  init.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
  init.loop_mode_enable = FALSE;
  init.priority = DMA_PRIORITY_HIGH;
  dma_init(dma->stream, &init);
}

static void spi_set_divider(spi_type *channel, spi_mclk_freq_div_type div) {
  if (div <= SPI_MCLK_DIV_256) {
    channel->ctrl2_bit.mdiv3en = FALSE;
    channel->ctrl2_bit.mdiv_h = FALSE;
    channel->ctrl1_bit.mdiv_l = div;
  } else if (div == SPI_MCLK_DIV_3) {
    channel->ctrl2_bit.mdiv3en = TRUE;
    channel->ctrl2_bit.mdiv_h = FALSE;
    channel->ctrl1_bit.mdiv_l = 0;
  } else {
    channel->ctrl2_bit.mdiv3en = FALSE;
    channel->ctrl2_bit.mdiv_h = TRUE;
    channel->ctrl1_bit.mdiv_l = div & 0x7;
  }
}

void spi_reconfigure(spi_bus_device_t *bus) {
  spi_device_t *config = &spi_dev[bus->port];
  const spi_port_def_t *port = &spi_port_defs[bus->port];
  const target_spi_port_t *dev = &target.spi_ports[bus->port];

  if (config->hz != bus->hz) {
    config->hz = bus->hz;
    spi_set_divider(port->channel, spi_find_divder(bus->hz));
  }
  if (config->mode != bus->mode) {
    config->mode = bus->mode;

    gpio_config_t gpio_init = gpio_config_default();
    gpio_init.mode = GPIO_ALTERNATE;
    gpio_init.drive = GPIO_DRIVE_HIGH;
    gpio_init.output = GPIO_PUSHPULL;

    if (bus->mode == SPI_MODE_LEADING_EDGE) {
      gpio_init.pull = GPIO_DOWN_PULL;
      gpio_pin_init_tag(dev->sck, gpio_init, SPI_TAG(bus->port, RES_SPI_SCK));

      port->channel->ctrl1_bit.clkpha = SPI_CLOCK_PHASE_1EDGE;
      port->channel->ctrl1_bit.clkpol = SPI_CLOCK_POLARITY_LOW;
    } else if (bus->mode == SPI_MODE_TRAILING_EDGE) {
      gpio_init.pull = GPIO_UP_PULL;
      gpio_pin_init_tag(dev->sck, gpio_init, SPI_TAG(bus->port, RES_SPI_SCK));

      port->channel->ctrl1_bit.clkpha = SPI_CLOCK_PHASE_2EDGE;
      port->channel->ctrl1_bit.clkpol = SPI_CLOCK_POLARITY_HIGH;
    }
  }
}

void spi_dma_transfer_begin(spi_ports_t port, uint8_t *buffer, uint32_t length, bool has_rx) {
  const dma_stream_def_t *dma_tx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_tx].dma];
  const dma_stream_def_t *dma_rx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_rx].dma];

  dma_clear_flag_tc(dma_rx);
  dma_clear_flag_tc(dma_tx);

  dma_prepare_rx_memory(buffer, length);
  dma_prepare_tx_memory(buffer, length);

  dma_rx->stream->maddr = (uint32_t)buffer;
  dma_data_number_set(dma_rx->stream, length);

  dma_tx->stream->maddr = (uint32_t)buffer;
  dma_data_number_set(dma_tx->stream, length);

  dma_interrupt_enable(dma_rx->stream, DMA_FDT_INT, TRUE);
  dma_interrupt_enable(dma_rx->stream, DMA_DTERR_INT, TRUE);

  dma_channel_enable(dma_rx->stream, TRUE);
  dma_channel_enable(dma_tx->stream, TRUE);

  spi_i2s_dma_transmitter_enable(spi_port_defs[port].channel, TRUE);
  spi_i2s_dma_receiver_enable(spi_port_defs[port].channel, TRUE);

  spi_enable(spi_port_defs[port].channel, TRUE);
}

void spi_device_init(spi_ports_t port) {
  const spi_port_def_t *def = &spi_port_defs[port];
  rcc_enable(def->rcc);

  const dma_stream_def_t *dma_rx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_rx].dma];
  const dma_stream_def_t *dma_tx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_tx].dma];

  dma_enable_rcc(dma_rx);
  dma_enable_rcc(dma_tx);

  spi_i2s_reset(def->channel);

  spi_init_type default_init;
  default_init.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX;
  default_init.master_slave_mode = SPI_MODE_MASTER;
  default_init.mclk_freq_division = SPI_MCLK_DIV_256;
  default_init.first_bit_transmission = SPI_FIRST_BIT_MSB;
  default_init.frame_bit_num = SPI_FRAME_8BIT;
  default_init.clock_polarity = SPI_CLOCK_POLARITY_HIGH;
  default_init.clock_phase = SPI_CLOCK_PHASE_2EDGE;
  default_init.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
  spi_init(def->channel, &default_init);

  spi_dev[port].dma_done = true;
  spi_dev[port].mode = SPI_MODE_TRAILING_EDGE;
  spi_dev[port].hz = 0;

  spi_dma_init_rx(port);
  spi_dma_init_tx(port);

  interrupt_enable(dma_rx->irq, DMA_PRIORITY);
}

void spi_seg_submit_wait_ex(spi_bus_device_t *bus, const spi_txn_segment_t *segs, const uint32_t count) {
  spi_txn_wait(bus);

  while (!spi_txn_can_send(bus, false))
    ;

  const spi_ports_t port = bus->port;

  spi_dev[port].dma_done = false;

  spi_reconfigure(bus);

  spi_csn_enable(bus);
  spi_enable(spi_port_defs[port].channel, TRUE);

  for (uint32_t i = 0; i < count; i++) {
    const spi_txn_segment_t *seg = &segs[i];
    const uint32_t size = seg->size;

    const uint8_t *tx_data = NULL;
    uint8_t *rx_data = NULL;

    if (seg->type == TXN_CONST) {
      tx_data = seg->bytes;
    } else {
      tx_data = seg->tx_data;
      rx_data = seg->rx_data;
    }

    for (uint32_t j = 0; j < size; j++) {
      while (!spi_i2s_flag_get(spi_port_defs[port].channel, SPI_I2S_TDBE_FLAG))
        ;

      spi_i2s_data_transmit(spi_port_defs[port].channel, tx_data ? tx_data[j] : 0xFF);

      while (!spi_i2s_flag_get(spi_port_defs[port].channel, SPI_I2S_RDBF_FLAG))
        ;

      const uint8_t ret = spi_i2s_data_receive(spi_port_defs[port].channel);
      if (rx_data != NULL) {
        rx_data[j] = ret;
      }
    }
  }

  spi_csn_disable(bus);
  spi_enable(spi_port_defs[port].channel, FALSE);

  spi_dev[port].dma_done = true;
}

static void handle_dma_rx_isr(spi_ports_t port) {
  const dma_stream_def_t *dma_tx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_tx].dma];
  const dma_stream_def_t *dma_rx = &dma_stream_defs[target.dma[spi_port_defs[port].dma_rx].dma];

  if (!dma_is_flag_active(dma_rx, DMA_FDT_FLAG)) {
    return;
  }

  dma_clear_flag_tc(dma_rx);
  dma_clear_flag_tc(dma_tx);

  dma_interrupt_enable(dma_rx->stream, DMA_FDT_INT, FALSE);
  dma_interrupt_enable(dma_rx->stream, DMA_DTERR_INT, FALSE);

  spi_i2s_dma_transmitter_enable(spi_port_defs[port].channel, FALSE);
  spi_i2s_dma_receiver_enable(spi_port_defs[port].channel, FALSE);

  dma_channel_enable(dma_rx->stream, FALSE);
  dma_channel_enable(dma_tx->stream, FALSE);

  spi_enable(spi_port_defs[port].channel, FALSE);

  spi_txn_finish(port);
}

void spi_dma_isr(const dma_device_t dev) {
  switch (dev) {
  case DMA_DEVICE_SPI1_RX:
    handle_dma_rx_isr(SPI_PORT1);
    break;

  case DMA_DEVICE_SPI2_RX:
    handle_dma_rx_isr(SPI_PORT2);
    break;

  case DMA_DEVICE_SPI3_RX:
    handle_dma_rx_isr(SPI_PORT3);
    break;

  case DMA_DEVICE_SPI4_RX:
    handle_dma_rx_isr(SPI_PORT4);
    break;

  default:
    break;
  }
}
