# SPDX-License-Identifier: Apache-2.0

set(REGION_SRCS "")
if(CONFIG_LORAWAN_CUSTOM_REGION_EU868)
  list(APPEND REGION_SRCS src/region/region_eu868.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_US915)
  list(APPEND REGION_SRCS src/region/region_us915.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_AU915)
  list(APPEND REGION_SRCS src/region/region_au915.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_CN470)
  list(APPEND REGION_SRCS src/region/region_cn470.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_KR920)
  list(APPEND REGION_SRCS src/region/region_kr920.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_IN865)
  list(APPEND REGION_SRCS src/region/region_in865.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_AS923)
  list(APPEND REGION_SRCS src/region/region_as923.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_CN779)
  list(APPEND REGION_SRCS src/region/region_cn779.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_RU864)
  list(APPEND REGION_SRCS src/region/region_ru864.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_EU433)
  list(APPEND REGION_SRCS src/region/region_eu433.c)
endif()

add_library(lorawan STATIC
    src/lorawan_impl.c
    src/crypto/aes128.c
    src/crypto/aes_cmac.c
    src/crypto/lorawan_crypto.c
    src/radio/radio_hal.c
    ${REGION_SRCS}
)

target_include_directories(lorawan PRIVATE src/)
target_link_libraries(lorawan PRIVATE zephyr_interface)
add_dependencies(lorawan zephyr_interface)
add_dependencies(lorawan zephyr_generated_headers)
