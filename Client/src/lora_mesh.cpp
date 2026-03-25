#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "lora_mesh.h"
#include "packet.h"

static const char* TAG = TAG_LORA;

static spi_device_handle_t g_spi_handle = NULL;
static bool g_initialized = false;
static uint8_t g_rx_buffer[MAX_PACKET_SIZE];
static uint8_t g_tx_buffer[MAX_PACKET_SIZE];

static void lora_cs_set(bool state)
{
    gpio_set_level((gpio_num_t)LORA_CS, state ? 1 : 0);
}

static uint8_t lora_spi_transfer(uint8_t data)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_data[0] = data;
    t.flags = SPI_TRANS_USE_TXDATA;
    
    spi_device_transmit(g_spi_handle, &t);
    return t.rx_data[0];
}

static void lora_write_reg(uint8_t reg, uint8_t value)
{
    lora_cs_set(0);
    lora_spi_transfer(reg);
    lora_spi_transfer(value);
    lora_cs_set(1);
}

static uint8_t lora_read_reg(uint8_t reg)
{
    lora_cs_set(0);
    lora_spi_transfer(reg);
    uint8_t value = lora_spi_transfer(0x00);
    lora_cs_set(1);
    return value;
}

static void lora_write_fifo(const uint8_t* data, uint8_t len)
{
    lora_cs_set(0);
    lora_spi_transfer(0x00);
    for (uint8_t i = 0; i < len; i++) {
        lora_spi_transfer(data[i]);
    }
    lora_cs_set(1);
}

static void lora_read_fifo(uint8_t* data, uint8_t len)
{
    lora_cs_set(0);
    lora_spi_transfer(0x00);
    for (uint8_t i = 0; i < len; i++) {
        data[i] = lora_spi_transfer(0x00);
    }
    lora_cs_set(1);
}

Status_t lora_mesh_init(void)
{
    if (g_initialized) return STATUS_OK;
    
    ESP_LOGI(TAG, "Initializing LoRa module...");
    
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.pin_bit_mask = (1ULL << LORA_CS) | (1ULL << LORA_RST) | (1ULL << LORA_DIO0);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    
    gpio_set_level((gpio_num_t)LORA_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)LORA_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    spi_bus_config_t bus_cfg;
    memset(&bus_cfg, 0, sizeof(bus_cfg));
    bus_cfg.mosi_io_num = LORA_MOSI;
    bus_cfg.miso_io_num = LORA_MISO;
    bus_cfg.sclk_io_num = LORA_SCK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = MAX_PACKET_SIZE;
    
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    
    spi_device_interface_config_t dev_cfg;
    memset(&dev_cfg, 0, sizeof(dev_cfg));
    dev_cfg.clock_speed_hz = 1000000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = -1;
    dev_cfg.queue_size = 1;
    
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &dev_cfg, &g_spi_handle));
    
    uint8_t version = lora_read_reg(0x42);
    ESP_LOGI(TAG, "LoRa version: 0x%02X", version);
    
    lora_write_reg(0x01, 0x00);
    lora_write_reg(0x01, 0x80);
    
    uint32_t frf = (uint32_t)((LORA_FREQ * 1000000.0f) / 32000000.0f * (1 << 19));
    lora_write_reg(0x06, (frf >> 16) & 0xFF);
    lora_write_reg(0x07, (frf >> 8) & 0xFF);
    lora_write_reg(0x08, frf & 0xFF);
    
    uint8_t bw_sf = 0x70 | ((LORA_SF - 6) << 4);
    lora_write_reg(0x1D, bw_sf);
    lora_write_reg(0x1E, 0x04 | (LORA_CR - 5));
    
    lora_write_reg(0x09, 0x40 | LORA_TX_POWER);
    lora_write_reg(0x0A, 0x00);
    
    lora_write_reg(0x20, (LORA_PREAMBLE >> 8) & 0xFF);
    lora_write_reg(0x21, LORA_PREAMBLE & 0xFF);
    
    lora_write_reg(0x0D, 0x00);
    lora_write_reg(0x0E, 0x00);
    
    lora_write_reg(0x40, 0x00);
    lora_write_reg(0x41, 0x00);
    
    lora_write_reg(0x01, 0x85);
    
    g_initialized = true;
    ESP_LOGI(TAG, "LoRa initialized");
    
    return STATUS_OK;
}

Status_t lora_mesh_send(const FullPacket_t* pkt)
{
    if (!g_initialized || !pkt) return STATUS_ERROR;
    
    size_t len = packet_serialize(pkt, g_tx_buffer, MAX_PACKET_SIZE);
    if (len <= 0) return STATUS_ERROR;
    
    lora_write_reg(0x01, 0x83);
    lora_write_reg(0x0D, 0x00);
    lora_write_fifo(g_tx_buffer, len);
    lora_write_reg(0x22, len);
    lora_write_reg(0x01, 0x83);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    lora_write_reg(0x01, 0x85);
    
    return STATUS_OK;
}

Status_t lora_mesh_receive(FullPacket_t* pkt)
{
    if (!g_initialized || !pkt) return STATUS_ERROR;
    
    uint8_t irq_flags = lora_read_reg(0x12);
    
    if (!(irq_flags & 0x40)) {
        return STATUS_TIMEOUT;
    }
    
    lora_write_reg(0x12, irq_flags);
    
    uint8_t rx_bytes = lora_read_reg(0x13);
    if (rx_bytes > MAX_PACKET_SIZE || rx_bytes < MIN_PACKET_SIZE) {
        return STATUS_INVALID;
    }
    
    lora_read_fifo(g_rx_buffer, rx_bytes);
    
    int result = packet_deserialize(g_rx_buffer, rx_bytes, pkt);
    if (result <= 0) return STATUS_INVALID;
    
    if (!packet_validate_crc(pkt)) return STATUS_INVALID;
    
    return STATUS_OK;
}

bool lora_mesh_available(void)
{
    uint8_t irq_flags = lora_read_reg(0x12);
    return (irq_flags & 0x40) ? true : false;
}

Status_t lora_mesh_send_ping(uint16_t dest)
{
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_PING, MY_NODE_ID, dest, 0);
    return lora_mesh_send(&pkt);
}

Status_t lora_mesh_send_pong(uint16_t dest)
{
    FullPacket_t pkt;
    packet_init_header(&pkt.header, PKT_PONG, MY_NODE_ID, dest, 0);
    return lora_mesh_send(&pkt);
}

void lora_mesh_deinit(void)
{
    g_initialized = false;
}