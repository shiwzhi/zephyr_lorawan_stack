# SPDX-License-Identifier: Apache-2.0

# Build lorawan as a static library so projects can link against it
add_library(lorawan STATIC
    lorawan_impl.c
    region/region.c
    crypto/aes128.c
    crypto/aes_cmac.c
    crypto/lorawan_crypto.c
    radio/radio_hal.c
    mac/mac_commands.c
    mac/downlink.c
    mac/rx.c
    mac/join.c
    region/region_cn470.c
    region/region_eu868.c
    region/region_us915.c
    region/region_au915.c
    region/region_kr920.c
    region/region_in865.c
    region/region_as923.c
    region/region_cn779.c
    region/region_ru864.c
)

target_include_directories(lorawan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# lorawan uses Zephyr kernel APIs (k_sleep, k_uptime_get, etc.)
target_link_libraries(lorawan PRIVATE zephyr_interface)
