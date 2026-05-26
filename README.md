# LoRaWAN Custom Stack

A standalone dual-version LoRaWAN end-device stack implementing the API
declared in `<zephyr/lorawan/lorawan.h>`, built on top of the raw Zephyr
LoRa driver (`<zephyr/drivers/lora.h>`).  Supports **LoRaWAN 1.0.3** and
**LoRaWAN 1.0.4**, with **Class A** and **Class C** operation.

## Architecture

```
src/lorawan/
├── common/                    ← shared across both versions
│   ├── crypto/                AES-128 encrypt, AES-CMAC, session keys, payload encrypt
│   │   ├── aes128.c / .h
│   │   ├── aes_cmac.c / .h
│   │   └── lorawan_crypto.c / .h
│   ├── radio/
│   │   ├── radio_hal.c / .h   configure TX/RX, build FHDR, get_data_channel
│   ├── mac/
│   │   ├── rx.c / .h          open_rx_windows (shared RX1/RX2 logic)
│   │   ├── downlink.c / .h    process_downlink, frame counter validation
│   │   └── mac_commands.h     forward-declarations for shared code
│   ├── region/
│   │   ├── region.h           struct region_ctx, dispatcher prototypes
│   │   ├── region_eu868.h     region-specific parameters (headers only)
│   │   ├── region_us915.h
│   │   ├── region_au915.h
│   │   ├── region_cn470.h
│   │   ├── region_cn779.h
│   │   ├── region_eu433.h
│   │   ├── region_kr920.h
│   │   ├── region_in865.h
│   │   ├── region_as923.h
│   │   └── region_ru864.h
│   └── lorawan_state.h        global g_ctx, enums, constants
│
├── v104/                      ← v1.0.4 implementation
│   ├── lorawan_impl.c         main MAC: join, send, receive, Class C, MAC cmds
│   ├── mac/
│   │   ├── mac_commands.c     CID dispatch (full 1.0.4 set)
│   │   └── join.c             DevNonce: counter-increment (NVS-persisted)
│   └── region/
│       ├── region.c           #ifdef-guarded dispatcher (no CN470, no EU433)
│       ├── region_eu868.c
│       ├── region_us915.c     NUM_DR=14, LR-FHSS placeholders (DR5/6)
│       ├── region_au915.c     NUM_DR=14, LR-FHSS placeholder (DR7)
│       ├── region_kr920.c
│       ├── region_in865.c
│       ├── region_as923.c
│       ├── region_cn779.c
│       └── region_ru864.c
│
└── v103/                      ← v1.0.3 implementation
    ├── lorawan_impl.c         main MAC (same structure as v104, minor_version=3)
    ├── mac/
    │   ├── mac_commands.c     CID dispatch (omits Reset, ADRParamSetup,
    │   │                       ForceRejoin, RejoinParamSetup)
    │   └── join.c             DevNonce: random (sys_rand32_get)
    └── region/
        ├── region.c           #ifdef-guarded dispatcher (includes CN470, EU433)
        ├── region_eu868.c
        ├── region_us915.c     NUM_DR=14, RP1-1.0.3 tables, DR5-7=RFU
        ├── region_au915.c     NUM_DR=14, RP1-1.0.3 tables, DR7=RFU
        ├── region_cn470.c     96 uplink channels, DR0-5
        ├── region_cn779.c
        ├── region_eu433.c     NEW: DR0-6, 3 default channels at 868 MHz
        ├── region_kr920.c
        ├── region_in865.c
        ├── region_as923.c
        └── region_ru864.c
```

Source files that differ between versions live in `v103/` and `v104/`.
Region header files (`.h`) live in `common/region/` and are shared.
Region implementation files (`.c`) live in each version's `region/` directory.

## Version & Region Selection (Kconfig)

The stack is configured entirely at **compile time** via Kconfig symbols.
Only the selected version and region are compiled — unselected regions
are excluded to save flash.

| Kconfig symbol | Values |
|----------------|--------|
| `LORAWAN_CUSTOM_VERSION` | `1_0_3` or `1_0_4` |
| `LORAWAN_CUSTOM_REGION` | AS923, AU915, CN470\*, CN779, **EU433**\*, EU868, KR920, IN865, US915, RU864 |

\* `CN470` and `EU433` depend on `LORAWAN_CUSTOM_VERSION_1_0_3`.

Example `prj.conf` for v1.0.3 on CN470:
```
CONFIG_LORAWAN_CUSTOM=y
CONFIG_LORAWAN_CUSTOM_VERSION_1_0_3=y
CONFIG_LORAWAN_CUSTOM_REGION_CN470=y
```

Example `prj.conf` for v1.0.4 on EU868:
```
CONFIG_LORAWAN_CUSTOM=y
CONFIG_LORAWAN_CUSTOM_VERSION_1_0_4=y
CONFIG_LORAWAN_CUSTOM_REGION_EU868=y
```

The CMakeLists.txt uses `add_lorawan_version()` to conditionally compile
region `.c` files based on `CONFIG_LORAWAN_CUSTOM_REGION_*`. A single
`lorawan` library alias points to the Kconfig-selected version.

## Key Features

### Both Versions
- OTAA and ABP activation
- Class A (RX1 + RX2 windows after each uplink)
- Class C (continuous RX with MAC command filtering on downlink,
  immediate ACK via delayed workqueue, poll-based pause/resume with no race)
- AES-128 encryption + AES-CMAC (RFC 4493), no external crypto lib
- Downlink frame counter replay protection
- Dwell-time config (AS923)
- MAC commands: LinkADRReq/Ans, RXParamSetupReq/Ans, NewChannelReq/Ans,
  DlChannelReq/Ans, DevStatusReq/Ans, DutyCycleReq/Ans,
  RXTimingSetupReq/Ans, TxParamSetupReq/Ans, LinkCheckReq/Ans,
  DeviceTimeReq/Ans

### v1.0.3 Specific
- **Random DevNonce** — `sys_rand32_get()` per join (§6.2.4)
- MAC commands: omits Reset (0x01), ADRParamSetup (0x0C),
  ForceRejoin (0x0E), RejoinParamSetup (0x0F)
- Region: **EU433** (7 DRs, 3 default channels at 868 MHz)
- CN470 channel plan (96ch uplink, DR0-5)

### v1.0.4 Specific
- **Persistent NVS DevNonce** — counter incremented across reboots
- Full MAC command set per LoRaWAN 1.0.4 L2 spec
- US915/AU915: NUM_DR=14, LR-FHSS placeholder entries for DR5/6/7

## Class C Details

- **Continuous RX**: a dedicated thread polls `class_c_active` and
  `class_c_paused` flags (100 ms sleep), then blocks on
  `k_sem_take(&class_c_data_sem, K_FOREVER)` for async data from the
  radio driver.
- **MAC filtering**: `process_downlink()` discards frames containing
  MAC commands when `is_class_c == true` (except the immediate ACK
  trigger).
- **Immediate ACK for confirmed downlinks**: a `class_c_ack_work`
  (delayed 100 ms) calls `lorawan_send(0, NULL, 0, UNCONFIRMED)` on
  the system workqueue within the 8-second Class C response window.
- **Pause/resume**: `lorawan_send()` sets `class_c_paused = true`,
  yields 100 ms for the Class C thread to exit RX, transmits, then
  clears the flag. No semaphore race.

## DevNonce Strategy

| Version | Strategy | Rationale |
|---------|----------|-----------|
| v1.0.3  | `sys_rand32_get()` | Per §6.2.4: DevNonce is random, 16-bit, probability of collision negligible for end-devices |
| v1.0.4  | Counter + NVS | Monotonic counter persisted across reboots |

## MAC Commands per Version

| CID | Command | v1.0.3 | v1.0.4 |
|-----|---------|--------|--------|
| 0x01 | Reset | — | ✓ |
| 0x02 | LinkCheck | ✓ | ✓ |
| 0x03 | LinkADR | ✓ | ✓ |
| 0x04 | DutyCycle | ✓ | ✓ |
| 0x05 | RXParamSetup | ✓ | ✓ |
| 0x06 | DevStatus | ✓ | ✓ |
| 0x07 | NewChannel | ✓ | ✓ |
| 0x08 | RXTimingSetup | ✓ | ✓ |
| 0x09 | TxParamSetup | ✓ | ✓ |
| 0x0A | DlChannel | ✓ | ✓ |
| 0x0B | DeviceTime | ✓ | ✓ |
| 0x0C | ADRParamSetup | — | ✓ |
| 0x0E | ForceRejoin | — | ✓ |
| 0x0F | RejoinParamSetup | — | ✓ |

## Per-Region Parameters

| Region | DRs | Uplink channels | Default channels | Notes |
|--------|-----|-----------------|------------------|-------|
| EU868 | 7 | 3 fixed + up to 16 | 868.1, 868.3, 868.5 MHz | |
| US915 | 14 | 72 (0-63 + 64-71) | 64 ch: 902.3-914.9 MHz | v104: LR-FHSS DR5/6 placeholder |
| CN470 | 6 | 96 | 470.3-489.7 MHz | v1.0.3 only |
| AU915 | 14 | 72 | 915.2-927.8 MHz | v104: LR-FHSS DR7 placeholder |
| KR920 | 5 | 8 | 920.9, 921.1, 921.3, 921.5, 921.7, 921.9 MHz | |
| IN865 | 6 | 3 | 865.0625, 865.4025, 865.985 MHz | |
| AS923 | 6 | 8 | 923.2, 923.4, 923.6, 923.8, 924.0, 924.2, 924.4, 924.6 MHz | Dwell-time configurable |
| CN779 | 6 | 3 | 779.5, 779.7, 779.9 MHz | |
| RU864 | 6 | 8 | 864.1, 864.3, 864.5, 864.7, 864.9, 865.1, 865.3, 865.5 MHz | |
| EU433 | 7 | 3 | 868.1, 868.3, 868.5 MHz | v1.0.3 only |

## Region Abstraction

`common/region/region.h` defines `struct region_ctx` which holds all
per-region state:

- Channel list (frequency, DR range, enable mask)
- DR-to-LoRa modem configuration table (SF + bandwidth)
- RX2 frequency and DR
- RX1/RX2 delays (seconds)
- TX power map (LinkADR power index 0-7 → dBm)
- Frequency band limits (`freq_min_hz`, `freq_max_hz`)
- Class C RXC parameters (`rxc_freq`, `rxc_dr` — default to RX2)

Each version's `region/region.c` dispatches to region-specific setup
functions wrapped in `#ifdef CONFIG_LORAWAN_CUSTOM_REGION_*`.  Adding
a new region requires a header in `common/region/`, a `.c` file in each
version's `region/` directory, and a case in both `region.c` files.

## Dependencies

- Zephyr RTOS (`k_sleep`, `k_uptime_get`, mutex, workqueue, logging)
- Zephyr LoRa driver (`lora_send`, `lora_recv`, `lora_config`)
- Zephyr NVS flash storage (v1.0.4 only)
- LoRa radio device (aliased as `lora0`)

## Usage

Minimal application flow:

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
CONFIG_LORAWAN_CUSTOM_VERSION_1_0_4=y
CONFIG_LORAWAN_CUSTOM_REGION_EU868=y
```

## Build System

`CMakeLists.txt` defines:

- **`lorawan_common`** — OBJECT library of shared files (crypto,
  radio_hal, rx, downlink)
- **`add_lorawan_version(name prefix)`** — creates a STATIC library
  `lorawan_${name}` from `${prefix}/lorawan_impl.c`,
  `${prefix}/mac/mac_commands.c`, `${prefix}/mac/join.c`,
  `${prefix}/region/region.c`, plus per-region `.c` files gated on
  `CONFIG_LORAWAN_CUSTOM_REGION_*`.
- **`lorawan`** — ALIAS pointing to the Kconfig-selected version
  (`lorawan_v103` or `lorawan_v104`).

Include path order (GCC `-I`):
1. Source-file directory (`.c` → finds its version's headers first)
2. `common/region/` (PRIVATE on each version library → finds `region_*.h`)
3. `common/` (PUBLIC from `lorawan_common` → finds `lorawan_state.h`,
   `radio/`, `crypto/`, `mac/`)

Only the selected version's library is linked.  The unselected version
and its region files are excluded at build time, saving flash.

## License

Apache-2.0
