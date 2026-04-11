# BTT TFT35-E3 Reflow Oven Controller

A DIY reflow oven controller that repurposes the **BIGTREETECH TFT35-E3 V3.0.1** 3D printer touchscreen as a **standalone reflow oven controller** -- no separate mainboard required.

The TFT35-E3 contains a GD32F205VCT6 (ARM Cortex-M3, 120MHz, 256KB flash, 128KB RAM) with a 3.5" color touchscreen, rotary encoder, SD card, USB, buzzer, and WS2812 LEDs. This project adds custom firmware modules to turn it into a full-featured reflow soldering station controller.

## Features

- **Full-color 3.5" touchscreen** with real-time temperature curve display
- **Reflow profile management** -- load/save profiles from SD card
- **PID temperature control** with configurable parameters and auto-tune
- **K-type thermocouple** via MAX6675/MAX31855 (SPI)
- **SSR control** for AC heating elements
- **Cooling fan PWM** for controlled cooldown
- **Buzzer alerts** for stage transitions
- **WS2812 LED status** -- green (ready), yellow (preheat), red (reflow), blue (cooling)
- **Rotary encoder** for quick adjustments without touching the screen
- **Built-in profiles** for leaded (Sn63/Pb37) and lead-free (SAC305) solder
- **Thermal runaway protection** with configurable limits

## Hardware Required

| Component | Specification | Notes |
|-----------|--------------|-------|
| BTT TFT35-E3 V3.0.1 | GD32F205 or STM32F207 variant | The controller |
| MAX6675 module | K-type thermocouple amplifier | SPI interface |
| K-type thermocouple | Up to 700C range | Oven temperature sensor |
| SSR-40DA | 3-32VDC input, 24-380VAC output | Switches heating elements |
| Toaster oven | ~1200-1500W | The oven itself |
| 5V USB power supply | Any phone charger (500mA+) | Powers the controller |
| 12V cooling fan | 80mm or 120mm | For controlled cooldown |
| NPN transistor (2N2222) | For SSR level shifting | Optional but recommended |
| Small N-channel MOSFET | For fan switching | e.g., IRLZ44N or 2N7000 |

**Total additional cost: ~$5** (transistor + MOSFET + misc)

See [docs/WIRING.md](docs/WIRING.md) for detailed wiring instructions.

## Pin Assignments

All connections use **existing headers** on the TFT35-E3 board -- no PCB trace cutting or fine-pitch soldering required.

| Function | MCU Pin | Board Connector | Header Pin |
|----------|---------|----------------|------------|
| Thermocouple SCK | PB10 | UART3 | TX pad |
| Thermocouple MISO | PB11 | UART3 | RX pad |
| Thermocouple CS | PA15 | FIL-DET | Signal pin |
| SSR Control | PC12 | PS-ON | Signal pin |
| Fan PWM | PD13 | EXP1/EXP3 | Pin 10 (BEEP) |
| Buzzer | PD13 | On-board | Shared with fan (see notes) |
| WiFi ESP32 (optional) | PA9/PA10 | WIFI header | TX1/RX1 |

> **Note:** The buzzer and fan share PD13. In practice, the buzzer is only used briefly between stages while the fan runs during cooldown. The firmware manages this multiplexing. Alternatively, use PA0 (UART4 TX) for the fan to keep them independent.

## Reflow Profiles

### Built-in Profiles

**Leaded (Sn63/Pb37)**
| Stage | Target | Ramp Rate | Hold Time |
|-------|--------|-----------|-----------|
| Preheat | 150C | 2.0 C/s | -- |
| Soak | 183C | 0.7 C/s | 60s |
| Reflow | 230C | 2.0 C/s | -- |
| Peak | 230C | hold | 15s |
| Cool | 50C | -3.0 C/s | -- |

**Lead-Free (SAC305)**
| Stage | Target | Ramp Rate | Hold Time |
|-------|--------|-----------|-----------|
| Preheat | 150C | 2.0 C/s | -- |
| Soak | 217C | 0.7 C/s | 90s |
| Reflow | 250C | 2.0 C/s | -- |
| Peak | 250C | hold | 15s |
| Cool | 50C | -3.0 C/s | -- |

Custom profiles can be created on the touchscreen or loaded from SD card (JSON format).

## Building the Firmware

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- Git

### Build

```bash
git clone https://github.com/Gabemedina23/btt-tft35-reflow.git
cd btt-tft35-reflow

# Build for GD32F205 variant (V3.0.1 board)
cd firmware-base
pio run -e BIGTREE_GD_TFT35_E3_V3_0

# Build for STM32F207 variant (V3.0 board)
pio run -e BIGTREE_TFT35_E3_V3_0
```

### Flashing

1. Copy the built `.bin` file to an SD card root
2. Copy the `TFT35/` folder (fonts + icons) to the SD card
3. Copy `config.ini` to the SD card
4. Insert SD card into the TFT35-E3's SD slot
5. Power cycle -- firmware flashes automatically (~10 seconds)

## Project Structure

```
btt-tft35-reflow/
├── README.md                 # This file
├── LICENSE                   # MIT License
├── docs/
│   ├── WIRING.md            # Detailed wiring guide with diagrams
│   ├── BOM.md               # Bill of materials with sourcing links
│   └── PROFILES.md          # Reflow profile format and tuning guide
├── hardware/
│   └── enclosure/           # 3D printable enclosure files (future)
├── firmware/
│   └── src/
│       └── Reflow/          # Custom reflow oven modules
│           ├── reflow_pins.h        # Pin definitions for reflow hardware
│           ├── max6675.h            # MAX6675 thermocouple driver
│           ├── max6675.c
│           ├── pid_controller.h     # PID controller with anti-windup
│           ├── pid_controller.c
│           ├── reflow_profile.h     # Reflow profile data structures
│           ├── reflow_profile.c
│           ├── reflow_control.h     # Main reflow state machine
│           ├── reflow_control.c
│           ├── reflow_menu.h        # Touchscreen UI menus
│           └── reflow_menu.c
├── profiles/
│   ├── leaded_sn63pb37.json  # Default leaded profile
│   └── leadfree_sac305.json  # Default lead-free profile
└── firmware-base/            # BTT TouchScreen firmware (git submodule)
```

## Heat Management: Dealing with Quartz Elements

Cheap toaster ovens use **quartz infrared heating elements** that emit intense radiant heat. This creates a problem for reflow: the direct IR radiation heats dark PCB surfaces much hotter than what the thermocouple reads from the air, which can char boards even when the sensor says the temperature is correct.

### The Problem

With no shielding, our first lead-free reflow charred the board. The thermocouple (measuring air temperature near the board) read ~60C lower than the actual board surface temperature due to direct IR radiation from the quartz elements.

### What We Tried

**1. Solid steel baking tray** (from the toaster's original accessories)
- Placed between the bottom elements and the board
- The steel absorbs IR and re-radiates as gentler convective heat
- Board temperature matched the reference thermometer well
- **Problem**: Steel has very high thermal mass. At 50% duty cycle, the tray stored so much heat that when the heater shut off, temperature coasted **35-80C past the target**. The PID autotune showed 534-second oscillation periods and 87C peak-to-valley swings. Calibration at 50C overshot to 83C with the heater completely off.

**2. Wire rack wrapped in aluminum foil** (current setup)
- Standard oven wire rack wrapped loosely in aluminum foil
- Foil reflects/absorbs IR from the quartz elements
- Wire rack allows some airflow underneath (less thermal mass than solid steel)
- A loose foil "tent" above the board shields from the top elements without trapping heat
- **Result**: 40% less overshoot, 30% tighter PID oscillation (60C amplitude vs 87C), 15% faster heat-up. Board temp still tracks the reference thermometer accurately.

### What Others Do

- **Controleo3 / Whizoo builds**: Solid aluminum tray (high thermal conductivity, even heating). They also use insulation (Floor & Tunnel Shield II) on the oven walls.
- **Commercial reflow ovens**: Perforated steel sheets to break up direct IR line-of-sight.
- **Some DIY builds**: Replace quartz elements entirely with slower-responding metal elements.

### Recommendations

Pick your approach based on your oven and priorities:

| Approach | Pros | Cons |
|----------|------|------|
| Aluminum foil on wire rack | Cheap, low thermal mass, easy to adjust | Foil may tear over time, less uniform than solid plate |
| Solid aluminum plate (~3mm) | Most uniform heating, best thermal conductivity | Adds thermal mass (better than steel though), costs more |
| Solid steel tray | Free (comes with oven), good IR blocking | Very high thermal mass, huge overshoot, hard to control |
| No shielding | Fastest heating | Will char boards, temperature readings don't match surface |

**If you change your heat diffuser setup, re-run the PID autotune.** The thermal characteristics change significantly between setups -- our steel tray PID values were 8x too aggressive for the old setup and still needed adjustment after switching to aluminum foil.

### Thermocouple Placement

For best accuracy, tape or clip the thermocouple probe to the board or tray surface with kapton tape, not dangling in the air. The air temperature inside the oven can be 30-70C different from the board surface depending on your shielding setup.

## How It Works

1. **The TFT35-E3 board IS the controller** -- no Arduino, no Raspberry Pi, no separate mainboard
2. MAX6675 reads oven temperature via software SPI on the UART3 header pins
3. PID controller computes heater duty cycle based on the reflow profile's current stage
4. SSR on the PS-ON header switches mains power to the heating elements
5. Cooling fan on EXP1 activates during cooldown stage
6. Touchscreen shows real-time temperature curve, current stage, and elapsed time
7. Profiles are stored on SD card and selectable via touch or encoder

## Safety

- **Thermal runaway protection**: If temperature exceeds profile peak + 20C, heater shuts off immediately
- **Timeout protection**: If any stage exceeds 3x expected duration, system aborts
- **Watchdog timer**: MCU hardware watchdog resets system if firmware hangs
- **SSR fails-open**: If controller loses power, SSR turns off (no current = open)
- **Manual override**: Encoder button press = emergency stop at any time

## Contributing

This project is in active development. Contributions welcome!

## License

MIT License -- see [LICENSE](LICENSE) file.

## Credits

- Based on [BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware) (MIT License)
- Reflow profile data from IPC/JEDEC J-STD-020 standards
- Inspired by [ReflowMaster](https://github.com/UnexpectedMaker/ReflowMaster) by Unexpected Maker
