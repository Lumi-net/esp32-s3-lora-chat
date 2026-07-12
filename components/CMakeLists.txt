set(src_dirs
            UART
            APP
            PWM
            SPI
            LCD
            FLASH
            UI
            IMAGES
            KEY)

set(include_dirs
            UART
            APP
            PWM
            SPI
            LCD
            FLASH
            UI
            IMAGES
            KEY)

set(requires
            driver
            esp_timer
            esp_driver_spi
            esp_lcd
            spi_flash
            nvs_flash
            lvgl)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)