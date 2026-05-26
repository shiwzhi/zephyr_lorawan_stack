# SPDX-License-Identifier: Apache-2.0

# ── Common object library (shared between v1.0.3 and v1.0.4) ──
add_library(lorawan_common OBJECT
    common/crypto/aes128.c
    common/crypto/aes_cmac.c
    common/crypto/lorawan_crypto.c
    common/radio/radio_hal.c
    common/mac/rx.c
    common/mac/downlink.c
)
target_include_directories(lorawan_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
target_link_libraries(lorawan_common PRIVATE zephyr_interface)
add_dependencies(lorawan_common zephyr_interface)

# ── Collect region source files based on Kconfig ──
function(add_lorawan_version name prefix)
    set(sources
        ${prefix}/lorawan_impl.c
        ${prefix}/mac/mac_commands.c
        ${prefix}/mac/join.c
        ${prefix}/region/region.c
    )

    if(CONFIG_LORAWAN_CUSTOM_REGION_AS923)
        list(APPEND sources ${prefix}/region/region_as923.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_AU915)
        list(APPEND sources ${prefix}/region/region_au915.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_CN470)
        list(APPEND sources ${prefix}/region/region_cn470.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_CN779)
        list(APPEND sources ${prefix}/region/region_cn779.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_EU868)
        list(APPEND sources ${prefix}/region/region_eu868.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_KR920)
        list(APPEND sources ${prefix}/region/region_kr920.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_IN865)
        list(APPEND sources ${prefix}/region/region_in865.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_US915)
        list(APPEND sources ${prefix}/region/region_us915.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_RU864)
        list(APPEND sources ${prefix}/region/region_ru864.c)
    endif()
    if(CONFIG_LORAWAN_CUSTOM_REGION_EU433)
        list(APPEND sources ${prefix}/region/region_eu433.c)
    endif()

    add_library(lorawan_${name} STATIC ${sources})
    target_include_directories(lorawan_${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/${prefix}
        ${CMAKE_CURRENT_SOURCE_DIR}/common/region
    )
    target_include_directories(lorawan_${name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common
    )
    target_link_libraries(lorawan_${name} PRIVATE lorawan_common zephyr_interface)
    add_dependencies(lorawan_${name} lorawan_common zephyr_interface)
endfunction()

# ── Build only the selected version ──
if(CONFIG_LORAWAN_CUSTOM_VERSION_1_0_4)
    add_lorawan_version(v104 v104)
    add_library(lorawan ALIAS lorawan_v104)
elseif(CONFIG_LORAWAN_CUSTOM_VERSION_1_0_3)
    add_lorawan_version(v103 v103)
    add_library(lorawan ALIAS lorawan_v103)
endif()
