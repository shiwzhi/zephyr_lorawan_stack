# LoRaWAN Custom Stack

A standalone LoRaWAN v1.0.3 end-device stack implementing the API
declared in `<zephyr/lorawan/lorawan.h>`, built on top of the raw Zephyr
LoRa driver (`<zephyr/drivers/lora.h>`). Supports **Class A** and
**Class C** operation.

## Architecture

```
src/lorawan/
├── CMakeLists.txt
├── Kconfig
├── README.md
└── src/
    ├── lorawan_impl.c          all MAC logic: join, send, RX windows,
    │                           MAC commands, downlink parsing, Class A/C
    ├── lorawan_state.h         global state struct, protocol constants
    ├── crypto/
    │   ├── aes128.h / .c       AES-128 ECB block encrypt
    │   ├── aes_cmac.h / .c     AES-CMAC (RFC 4493)
    │   └── lorawan_crypto.h / .c  MIC, payload encrypt, session key derivation
    ├── radio/
    │   └── radio_hal.h / .c    TX/RX config, FHDR builder, channel selection
    └── region/
        ├── region.h            region_ctx struct, generic inline helpers
        └── region_xxxxx.c      10 self-contained region implementations
                                (one compiled per build via Kconfig)
```

All MAC-layer logic (join procedure, uplink framing, RX1/RX2 window
handling, downlink parsing, MAC command processing, and Class A/C
state management) is consolidated in `src/lorawan_impl.c`.

Each `src/region/region_xxxxx.c` is self-contained: it includes
`region.h`, defines its own data tables, and implements all region
API functions (`region_init`, `region_get_rx1_dr`, etc.). Only the
selected region's `.c` is compiled — no dispatcher, no `#ifdef` soup.
Generic helpers (`region_dr_to_lora`, `region_get_join_channel`,
`region_cflist_type_a/b`) are `static inline` in `region.h`.

## Region Selection (Kconfig)

The stack is configured at **compile time** via Kconfig symbols.
Only the selected region is compiled — unselected regions are excluded
to save flash.

| Kconfig symbol | Values |
|----------------|--------|
| `LORAWAN_CUSTOM_REGION` | AS923, AU915, CN470, CN779, EU433, EU868, KR920, IN865, US915, RU864 |

Example `prj.conf`:
```
CONFIG_LORAWAN_CUSTOM=y
CONFIG_LORAWAN_CUSTOM_REGION_CN470=y
```

## Key Features

- OTAA and ABP activation
- Class A (RX1 + RX2 windows after each uplink)
- Class C (continuous RX with MAC command filtering on downlink,
  immediate ACK via delayed workqueue, poll-based pause/resume)
- AES-128 encryption + AES-CMAC (RFC 4493), no external crypto lib
- Downlink frame counter replay protection
- Dwell-time config (AS923)
- **Random DevNonce** — `sys_rand32_get()` per join (§6.2.4)
- MAC commands: LinkADRReq/Ans, RXParamSetupReq/Ans, NewChannelReq/Ans,
  DlChannelReq/Ans, DevStatusReq/Ans, DutyCycleReq/Ans,
  RXTimingSetupReq/Ans, TxParamSetupReq/Ans, LinkCheckReq/Ans,
  DeviceTimeReq/Ans
- Regions: CN470, EU433, EU868, US915, AU915, KR920, IN865, AS923,
  CN779, RU864

## Class C Details

- **Continuous RX**: a dedicated thread polls `class_c_active` and
  `class_c_paused` flags (100 ms sleep), then blocks on
  `k_sem_take(&class_c_data_sem, K_FOREVER)` for async data.
- **MAC filtering**: `process_downlink()` discards frames containing
  MAC commands when `is_class_c == true` (except the immediate ACK
  trigger).
- **Immediate ACK for confirmed downlinks**: a `class_c_ack_work`
  (delayed 100 ms) sends an unconfirmed uplink within the 8-second
  Class C response window.
- **Pause/resume**: `lorawan_send()` sets `class_c_paused = true`,
  yields 100 ms for the Class C thread to exit RX, transmits, then
  clears the flag.

## Per-Region Parameters

| Region | DRs | Uplink channels | Default channels |
|--------|-----|-----------------|------------------|
| EU868 | 7 | 3 fixed + up to 16 | 868.1, 868.3, 868.5 MHz |
| US915 | 14 | 72 (0-63 + 64-71) | 64 ch: 902.3-914.9 MHz |
| CN470 | 6 | 96 | 470.3-489.7 MHz |
| AU915 | 14 | 72 | 915.2-927.8 MHz |
| KR920 | 5 | 8 | 920.9-923.5 MHz |
| IN865 | 6 | 3 | 865.0625, 865.4025, 865.985 MHz |
| AS923 | 6 | 8 | 923.2-924.6 MHz |
| CN779 | 6 | 3 | 779.5, 779.7, 779.9 MHz |
| RU864 | 6 | 8 | 864.1-865.5 MHz |
| EU433 | 7 | 3 | 433.175, 433.375, 433.575 MHz |

## Region Abstraction

`src/region/region.h` defines `struct region_ctx` which holds all
per-region state:

- Channel list (frequency, DR range, enable mask)
- DR-to-LoRa modem configuration table (SF + bandwidth)
- RX2 frequency and DR
- RX1/RX2 delays (seconds)
- TX power map (LinkADR power index 0-7 → dBm)
- Frequency band limits (`freq_min_hz`, `freq_max_hz`)
- Class C RXC parameters (`rxc_freq`, `rxc_dr` — default to RX2)

`src/region/region.c` dispatches to region-specific implementations.
Adding a new region requires a single `.c` file in `src/region/`
and a Kconfig entry.

## Dependencies

- Zephyr RTOS (`k_sleep`, `k_uptime_get`, mutex, workqueue, logging)
- Zephyr LoRa driver (`lora_send`, `lora_recv`, `lora_config`)
- LoRa radio device (aliased as `lora0`)

## Usage

```c
lorawan_set_region(LORAWAN_REGION_EU868);
lorawan_start();
lorawan_join(&join_cfg);  // OTAA
lorawan_send(port, data, len, LORAWAN_MSG_UNCONFIRMED);
```

Kconfig in `prj.conf`:
```
CONFIG_LORA=y
CONFIG_LORAWAN=n                          // disable Zephyr's built-in
CONFIG_LORA_MODULE_BACKEND_NATIVE=y

CONFIG_LORAWAN_CUSTOM=y
CONFIG_LORAWAN_CUSTOM_REGION_EU868=y
```

## Build System

`CMakeLists.txt` defines a single STATIC library with conditional
region compilation:

```cmake
set(REGION_SRCS "")
if(CONFIG_LORAWAN_CUSTOM_REGION_EU868)
  list(APPEND REGION_SRCS src/region/region_eu868.c)
elseif(CONFIG_LORAWAN_CUSTOM_REGION_US915)
  list(APPEND REGION_SRCS src/region/region_us915.c)
# ... one per region
endif()

add_library(lorawan STATIC
    src/lorawan_impl.c
    src/crypto/aes128.c src/crypto/aes_cmac.c src/crypto/lorawan_crypto.c
    src/radio/radio_hal.c
    ${REGION_SRCS}
)
target_include_directories(lorawan PRIVATE src/)
```

Only the selected region's `.c` is compiled and linked.

Include path order (GCC `-I`):
1. `src/` (PRIVATE) — finds all headers uniformly

## License

Apache-2.0
