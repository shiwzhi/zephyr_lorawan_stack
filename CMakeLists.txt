# SPDX-License-Identifier: Apache-2.0

add_library(lorawan STATIC
    src/lorawan_impl.c
    src/crypto/aes128.c
    src/crypto/aes_cmac.c
    src/crypto/lorawan_crypto.c
    src/radio/radio_hal.c
    src/region/region.c
)

target_include_directories(lorawan PUBLIC include/
    PRIVATE include/crypto include/radio include/region)
target_link_libraries(lorawan PRIVATE zephyr_interface)
add_dependencies(lorawan zephyr_interface)
