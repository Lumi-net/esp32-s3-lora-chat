#include "spi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

void spi_init()
{
    spi_bus_config_t spibus_structure = {
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .max_transfer_sz = 240*240*2,
        .miso_io_num = GPIO_NUM_NC,
        .mosi_io_num = GPIO_NUM_11,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
        .sclk_io_num = GPIO_NUM_12,
    };
    spi_bus_initialize(SPI2_HOST, &spibus_structure, SPI_DMA_CH_AUTO);
}