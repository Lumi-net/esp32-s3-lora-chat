set(src_dirs
            UART
            APP
            PWM
            SPI
            LCD
            FLASH
            MYNVS
            UI
            IMAGES
            WIFI
            FONTS
            SLEEP
            KEY)

set(include_dirs
            UART
            APP
            PWM
            SPI
            LCD
            FLASH
            MYNVS
            UI
            IMAGES
            WIFI
            FONTS
            SLEEP
            KEY)

set(requires
            driver
            esp_timer
            esp_driver_spi
            esp_lcd
            spi_flash
            nvs_flash
            esp_wifi
            esp_http_server
            lwip
            lvgl)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

# component_compile_options(-ffast-math -O2 -Wno-error=format=-Wno-format)