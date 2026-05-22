# LoRaWAN Custom Stack

A standalone LoRaWAN Class A end-device stack implementing the API declared in `<zephyr/lorawan/lorawan.h>`, built on top of the raw Zephyr LoRa driver (`<zephyr/drivers/lora.h>`).

## Architecture

```
lorawan_impl.c           → LoRaWAN MAC layer (join, send, receive, MIC, encryption)
region/region.h / .c     → Region abstraction dispatcher
region/region_*.h / .c   → Per-region parameters (DR tables, frequencies, payload limits)
```

| File | Purpose |
|------|---------|
| `lorawan_impl.c` | Full LoRaWAN stack: OTAA join, uplink/downlink frames, AES-128 encryption, AES-CMAC MIC, RX1/RX2 windows, MAC command processing |
| `region/region.c` | Dispatches to region-specific implementations based on `enum lorawan_region` |
| `region/region_cn470.c/h` | CN470-510 (China) — 96 uplink channels, DR0–DR5 |
| `region/region_eu868.c/h` | EU868 (Europe) — 3 default channels, DR0–DR6 |
| `region/region_us915.c/h` | US915 (US) — 72 uplink channels, DR0–DR9 |
| `region/region_au915.c/h` | AU915 (Australia) — 72 uplink channels, DR0–DR9 |
| `region/region_kr920.c/h` | KR920 (Korea) — 8 channels, DR0–DR4 |
| `region/region_in865.c/h` | IN865 (India) — 3 channels, DR0–DR5 |
| `region/region_as923.c/h` | AS923 (Asia) — 8 channels, DR0–DR5 |
| `region/region_cn779.c/h` | CN779-787 (China) — 3 channels, DR0–DR5 |
| `region/region_ru864.c/h` | RU864 (Russia) — 8 channels, DR0–DR5 |

## Key Features

- **LoRaWAN 1.0.4 compatible** — uses a single `app_key` for all join and session key derivation (1.0.4 style). The stack derives both `NwkSKey` (`0x01`) and `AppSKey` (`0x02`) from the root `AppKey`.
- **OTAA only** — join via Over-The-Air Activation; ABP is also supported.
- **Class A** — bidirectional end-device with two receive windows (RX1, RX2) after each uplink.
- **AES-128 from scratch** — standalone AES-128 encrypt + AES-CMAC (RFC4493), no external crypto library.
- **MAC commands** — full 1.0.4 MAC command processing: LinkADRReq, RXParamSetupReq, NewChannelReq, DlChannelReq, DevStatusReq, DutyCycleReq, RXTimingSetupReq, TxParamSetupReq, ResetConf, RejoinParamSetupReq, ForceRejoinReq, DeviceTimeAns, ADRParamSetupReq.
- **Persistent DevNonce** — stored in NVS flash across reboots to guarantee monotonicity.
- **Replay protection** — downlink frame counter is validated strictly.

## Region Abstraction

`region.h` defines `struct region_ctx` which holds all per-region state:

- Channel list (frequency, DR range, enable mask)
- DR-to-LoRa modem configuration table
- RX2 frequency and DR
- RX1/RX2 delays
- Frequency band limits (`freq_min_hz`, `freq_max_hz`)
- Max channel count (`max_channels`)

Each `region/region_*.c` file fills this struct with region-specific values. Adding a new region requires a new header/source pair and a case in `region_init()`.

## Dependencies

- Zephyr RTOS (`k_sleep`, `k_uptime_get`, mutex, logging)
- Zephyr LoRa driver (`lora_send`, `lora_recv`, `lora_config`)
- Zephyr NVS flash storage
- LoRa radio device (aliased as `lora0`, `zephyr,lorawan-transceiver`, or `lora`)

## Usage

In `prj.conf`:
```
CONFIG_LORA=y
# Do NOT set CONFIG_LORAWAN=y — this stack replaces the built-in subsystem
```

Minimal application flow:
```c
lorawan_set_region(LORAWAN_REGION_CN470);
lorawan_start();
lorawan_join(&join_cfg);  // OTAA
lorawan_send(port, data, len, LORAWAN_MSG_UNCONFIRMED);
```

## Building

This directory builds as a static library `lorawan` via CMake. Add to your project with:
```cmake
add_subdirectory(src/lorawan)
target_link_libraries(app PRIVATE lorawan)
```

## License

Apache-2.0
