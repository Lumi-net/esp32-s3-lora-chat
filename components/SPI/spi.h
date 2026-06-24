#ifndef __MYSPI_H__
#define __MYSPI_H__

#include "driver/gpio.h"
#include "driver/spi_master.h"

extern spi_device_handle_t spi2_handle;
void spi_init();

#endif