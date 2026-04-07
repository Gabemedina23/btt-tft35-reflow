# Wiring Guide

## Overview

All connections use existing headers on the BTT TFT35-E3 V3.0.1 board. No trace cutting or fine-pitch soldering is required.

```
                    BTT TFT35-E3 V3.0.1
                    ┌─────────────────────┐
                    │                     │
   MAX6675 ◄──SPI──┤ UART3 header        │
   (thermocouple)   │ (PB10/PB11)         │
                    │                     │
                    │ FIL-DET header      │
   MAX6675 CS ◄─────┤ (PA15)              │
                    │                     │
   SSR-40DA ◄───────┤ PS-ON header        │
   (heater)         │ (PC12)              │
                    │                     │
   Fan + MOSFET ◄───┤ UART4 header        │
   (cooling)        │ (PA0)               │
                    │                     │
   ESP32 (opt) ◄────┤ WIFI header         │
   (WiFi)           │ (PA9/PA10)          │
                    │                     │
   5V Power ────────┤ USB / RS232 header  │
                    └─────────────────────┘
```

## Power Supply

The TFT35-E3 runs on **5V DC** and draws ~200-350mA. Any USB phone charger works.

**Options:**
- Micro-USB cable (simplest)
- 5V to RS232 connector pin 1 (+5V) and pin 2 (GND)

> **Important:** The heating elements are powered directly from mains (120/240VAC) through the SSR. The 5V supply only powers the controller board.

## 1. MAX6675 Thermocouple Module

The MAX6675 communicates via SPI (read-only, no MOSI needed). We use software SPI on the UART3 header.

### Wiring

| MAX6675 Pin | Wire To | TFT35-E3 Location |
|-------------|---------|-------------------|
| VCC | 3.3V | FIL-DET header pin 3 |
| GND | GND | UART3 header pin 1 |
| SCK | PB10 | UART3 header pin 3 (TX) |
| SO (MISO) | PB11 | UART3 header pin 2 (RX) |
| CS | PA15 | FIL-DET header pin 1 (signal) |

### UART3 Header Pinout (bottom edge of board)

```
[GND] [RX3/PB11] [TX3/PB10]
  1       2          3
```

### FIL-DET Header Pinout (bottom edge of board)

```
[PA15] [GND] [3.3V]
  1      2     3
```

### Thermocouple Placement

- Place the thermocouple probe **near the center of the oven**, at PCB height
- Use a small clip or high-temp tape to secure it to the oven rack
- Keep the thermocouple wire away from heating elements
- Route wires out through the oven door seal or a small hole in the back

## 2. SSR-40DA (Solid State Relay)

The SSR switches mains power to the oven heating elements. The TFT35-E3's PS-ON pin controls it.

### SSR Input (DC Side)

The SSR-40DA input is rated 3-32VDC. At 3.3V GPIO output, it's marginal. **Recommended: add an NPN transistor for reliable switching.**

#### Option A: Direct GPIO (may work, not guaranteed)

| SSR Pin | Wire To |
|---------|---------|
| DC Input + (pin 3) | 3.3V (from FIL-DET pin 3) |
| DC Input - (pin 4) | PC12 (PS-ON header pin 1) |

> In this configuration, PC12 LOW = SSR ON, PC12 HIGH = SSR OFF (inverted logic).

#### Option B: NPN Transistor (recommended)

```
                 5V (from RS232 header pin 1)
                  │
                  ├──── SSR DC+ (pin 3)
                  │
                  │    SSR DC- (pin 4)
                  │         │
                  │     ┌───┘
                  │     │ C
PC12 ──[1K]──────┤B        2N2222
                  │     │ E
                  │     └───┐
                  │         │
                 GND       GND
```

| Component | Connection |
|-----------|-----------|
| PC12 (PS-ON pin 1) | 1K resistor to 2N2222 base |
| 2N2222 collector | SSR DC- (pin 4) |
| 2N2222 emitter | GND |
| SSR DC+ (pin 3) | 5V (RS232 header pin 1) |

Logic: PC12 HIGH = transistor ON = SSR ON = heater ON.

### SSR Output (AC Side)

**CAUTION: MAINS VOLTAGE - DANGEROUS**

```
Mains Live ──── Oven heating element (one leg)
                     │
                (other leg) ──── SSR AC pin 1
                                      │
                               SSR AC pin 2 ──── Mains Neutral
```

### SSR Mounting

- Mount the SSR on a **heatsink** (aluminum plate or the oven's exterior metal)
- Use thermal paste between SSR and heatsink
- The SSR-40DA can get warm even with a heatsink when switching 10A+ loads
- Keep SSR wiring away from the thermocouple wires (EMI)

## 3. Cooling Fan

A 12V fan assists with controlled cooldown after reflow. Since the TFT35-E3 only has 3.3V/5V rails, you need an external 12V supply and a MOSFET to switch it.

### Wiring

```
12V supply (+) ──── Fan (+)
                      │
                 Fan (-) ──── MOSFET Drain
                              MOSFET Gate ──[1K]── PA0 (UART4 TX pin 3)
                              MOSFET Source ──── GND (UART4 pin 1)
                              MOSFET Gate ──[10K]── GND (pull-down)
```

### UART4 Header Pinout (bottom edge of board)

```
[GND] [RX4/PC11] [TX4/PC10 or PA0]
  1       2          3
```

> **Note:** Check your board's actual UART4 TX pin. The pin file shows PA0 and PC10 as alternates. Use whichever is broken out on the header.

### MOSFET Selection

Any logic-level N-channel MOSFET works:
- **IRLZ44N** — overkill but common, Vgs(th) 1-2V
- **2N7000** — TO-92 package, handles 200mA (enough for one fan)
- **IRLML6344** — SMD, Vgs(th) 0.8V, handles 5A

## 4. ESP32 WiFi Bridge (Optional)

Connect an ESP32 to the WIFI header for remote temperature monitoring and profile upload via web browser.

### WIFI Header Pinout

```
[3.3V] [TX1/PA9] [PD3] [PD2] [NC] [NC] [RX1/PA10] [GND]
  1       2        3     4     5    6      7          8
```

| ESP32 Pin | Wire To | TFT35-E3 WIFI Header |
|-----------|---------|---------------------|
| 3.3V | 3.3V | Pin 1 |
| GND | GND | Pin 8 |
| RX | PA9 (TX1) | Pin 2 |
| TX | PA10 (RX1) | Pin 7 |

The firmware sends temperature data over UART1 as JSON. The ESP32 runs a simple web server to display it.

## 5. Complete Wiring Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                        MAINS (120/240VAC)                       │
│                             │     │                              │
│                          ┌──┘     └──┐                           │
│                          │  SSR-40DA  │                           │
│                          │  AC Side   │                           │
│                          └──┬─────┬──┘                           │
│                             │     │                              │
│                        Oven Heating Elements                     │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│                       LOW VOLTAGE (5V/3.3V/12V)                  │
│                                                                  │
│  ┌─────────────┐     ┌───────────────────────┐    ┌──────────┐  │
│  │   MAX6675   │     │   BTT TFT35-E3 V3.0.1 │    │ SSR-40DA │  │
│  │             │     │                       │    │ DC Side  │  │
│  │  VCC──3.3V──┼─────┤ FIL-DET 3.3V         │    │          │  │
│  │  GND──GND───┼─────┤ UART3 GND            │    │  (+)──5V─┼──┤
│  │  SCK──PB10──┼─────┤ UART3 TX             │    │  (-)─────┼──┤
│  │  SO───PB11──┼─────┤ UART3 RX             │    │     ▲    │  │
│  │  CS───PA15──┼─────┤ FIL-DET signal        │    │     │    │  │
│  └─────────────┘     │                       │    └─────┼────┘  │
│                      │ PS-ON (PC12)──[1K]──B─┤──2N2222──C       │
│  ┌─────────────┐     │                    E──┼──GND              │
│  │  12V Fan    │     │                       │                   │
│  │  (+)──12V───┼─    │ UART4 TX (PA0)──[1K]─┼──MOSFET Gate     │
│  │  (-)────────┼─────┤                  GND──┼──MOSFET Source   │
│  └─────────────┘     │                       │  MOSFET Drain──Fan│
│                      │      ┌──USB 5V──┐     │                   │
│                      │      └──Power───┘     │                   │
│                      └───────────────────────┘                   │
└──────────────────────────────────────────────────────────────────┘
```

## Safety Checklist

- [ ] All mains wiring uses appropriately rated wire (14 AWG minimum for 15A)
- [ ] SSR mounted on heatsink with thermal paste
- [ ] Mains connections are insulated (heat shrink or terminal blocks)
- [ ] Controller board is outside the oven (not inside the heated cavity)
- [ ] Thermocouple wire is high-temperature rated
- [ ] Emergency stop accessible (encoder button or power switch)
- [ ] GFCI outlet used for mains connection
- [ ] Never leave unattended during reflow cycle
