# Bill of Materials

## Core Components (Required)

| # | Component | Specification | Qty | Have It? | Est. Cost | Notes |
|---|-----------|--------------|-----|----------|-----------|-------|
| 1 | BTT TFT35-E3 V3.0.1 | GD32F205 or STM32F207 | 1 | Yes | $25 | The controller |
| 2 | MAX6675 Module | K-type thermocouple amplifier, SPI | 1 | Yes | $4 | Breakout board with header pins |
| 3 | K-type Thermocouple | Ring or probe tip, fiberglass insulated | 1 | Yes | $3 | Included with most MAX6675 modules |
| 4 | SSR-40DA | 3-32VDC in, 24-380VAC out, 40A | 1 | Yes | $8 | Inkbird or Fotek brand |
| 5 | Toaster Oven | 1200-1500W, quartz tube elements | 1 | Yes | $30-50 | Already disassembled and insulated |
| 6 | 5V USB Power Supply | 500mA minimum | 1 | Yes | $0 | Any phone charger |

## Level Shifting & Switching (Recommended)

| # | Component | Specification | Qty | Est. Cost | Notes |
|---|-----------|--------------|-----|-----------|-------|
| 7 | 2N2222 NPN Transistor | TO-92, general purpose | 1 | $0.05 | SSR level shift (3.3V GPIO to 5V SSR) |
| 8 | 1K Resistor | 1/4W, through-hole | 2 | $0.02 | Base resistors for transistor/MOSFET |
| 9 | 10K Resistor | 1/4W, through-hole | 1 | $0.01 | MOSFET gate pull-down |
| 10 | Logic-level N-MOSFET | IRLZ44N, 2N7000, or similar | 1 | $0.50 | Fan switching (12V from 3.3V logic) |

## Cooling System

| # | Component | Specification | Qty | Est. Cost | Notes |
|---|-----------|--------------|-----|-----------|-------|
| 11 | 12V DC Fan | 80mm or 120mm, brushless | 1 | $5 | For controlled cooldown |
| 12 | 12V Power Supply | 1A, barrel jack or wire leads | 1 | $5 | Powers fan only (or share with oven fan) |

## Oven Modifications

| # | Component | Specification | Qty | Est. Cost | Notes |
|---|-----------|--------------|-----|-----------|-------|
| 13 | Reflective Insulation | Aluminum-backed fiberglass | 1 roll | $5 | Line oven interior for better heat retention |
| 14 | High-temp Silicone Wire | 14 AWG, 200C rated | 3 ft | $3 | For SSR to element connections |
| 15 | SSR Heatsink | Aluminum, drilled for SSR | 1 | $3 | Or use oven exterior as heatsink |
| 16 | Ring Terminals | For mains wiring connections | 4 | $1 | Crimp-on, insulated |
| 17 | Thermal Paste | Any CPU thermal paste | small | $0 | For SSR to heatsink |

## Optional

| # | Component | Specification | Qty | Est. Cost | Notes |
|---|-----------|--------------|-----|-----------|-------|
| 18 | ESP32 DevKit | WROOM-32 | 1 | $5 | WiFi monitoring (optional) |
| 19 | Servo Motor | SG90 or MG90S | 1 | $3 | Auto door opener for faster cooling |
| 20 | Second Thermocouple | K-type + MAX6675 | 1 | $7 | Dual-zone monitoring |

## Wire & Connectors

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 21 | Dupont jumper wires (F-F) | 10 | For TFT35 header connections |
| 22 | JST-XH or Dupont connectors | assorted | If making custom cables |
| 23 | Heat shrink tubing | assorted | For mains wire insulation |
| 24 | Wire nuts or Wago connectors | 4 | For mains connections |

## Total Estimated Cost

| Category | Cost |
|----------|------|
| Core (already have) | $0 |
| Level shifting | ~$1 |
| Cooling | ~$10 |
| Oven mods | ~$12 |
| **Total new purchases** | **~$23** |
