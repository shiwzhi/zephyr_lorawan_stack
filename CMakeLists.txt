# SPDX-License-Identifier: Apache-2.0

# Build lorawan as a static library so projects can link against it
add_library(lorawan STATIC
    lorawan_impl.c
    region.c
    region_cn470.c
    region_eu868.c
    region_us915.c
    region_au915.c
    region_kr920.c
    region_in865.c
    region_as923.c
    region_cn779.c
    region_ru864.c
)

target_include_directories(lorawan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# lorawan uses Zephyr kernel APIs (k_sleep, k_uptime_get, etc.)
target_link_libraries(lorawan PRIVATE zephyr_interface)
