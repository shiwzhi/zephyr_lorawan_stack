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
├── include/                    ← all public headers
│   ├── lorawan_state.h         global state struct, protocol constants
│   ├── crypto/
│   │   ├── aes128.h            AES-128 ECB block encrypt
│   │   ├── aes_cmac.h          AES-CMAC (RFC 4493)
│   │   └── lorawan_crypto.h    MIC, payload encrypt, session key derivation
│   ├── radio/
│   │   └── radio_hal.h         TX/RX config, FHDR builder, channel selection
│   └── region/
│       ├── region.h            region_ctx struct, dispatcher API
│       └── region_*.h          10 region headers (self-contained, header-only)
└── src/                        ← implementation
    ├── lorawan_impl.c          all MAC logic: join, send, RX windows,
    │                           MAC commands, downlink parsing, Class A/C
    ├── crypto/
    │   ├── aes128.c
    │   ├── aes_cmac.c
    │   └── lorawan_crypto.c
    ├── radio/
    │   └── radio_hal.c
    └── region/
        └── region.c            region dispatcher (#ifdef per-region)
```

All MAC-layer logic (join procedure, uplink framing, RX1/RX2 window
handling, downlink parsing, MAC command processing, and Class A/C
state management) is consolidated in `src/lorawan_impl.c`.

Region headers (`include/region/region_*.h`) are **header-only**: each
contains `static const` data tables and `static inline` helper functions.
No separate region `.c` files are needed. Only the selected region is
compiled via `#ifdef` guards in `src/region/region.c`.

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

`include/region/region.h` defines `struct region_ctx` which holds all
per-region state:

- Channel list (frequency, DR range, enable mask)
- DR-to-LoRa modem configuration table (SF + bandwidth)
- RX2 frequency and DR
- RX1/RX2 delays (seconds)
- TX power map (LinkADR power index 0-7 → dBm)
- Frequency band limits (`freq_min_hz`, `freq_max_hz`)
- Class C RXC parameters (`rxc_freq`, `rxc_dr` — default to RX2)

`src/region/region.c` dispatches to region-specific setup functions
via `#ifdef CONFIG_LORAWAN_CUSTOM_REGION_*`. Adding a new region requires
a single header in `include/region/`.

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

`CMakeLists.txt` defines a single STATIC library:

```cmake
add_library(lorawan STATIC
    src/lorawan_impl.c
    src/crypto/aes128.c src/crypto/aes_cmac.c src/crypto/lorawan_crypto.c
    src/radio/radio_hal.c
    src/region/region.c
)
target_include_directories(lorawan PUBLIC include/
    PRIVATE include/crypto include/radio include/region)
```

No per-region `.c` files to list. The Kconfig `#ifdef`s in `region.c`
handle region selection automatically.

Include path order (GCC `-I`):
1. `include/` (PUBLIC) — finds all headers uniformly

## License

Apache-2.0
