# Portable Power Station

This repository contains the hardware and software design files for our Senior Design Project at UTRGV. p

## Overview
Current portable power stations often suffer from poor State-of-Charge (SOC) estimation, inaccurate runtime predictions, and binary failure modes. Our primary objective is to solve this by designing and building an intelligent portable power station that delivers accurate, predictive battery statistics alongside early warnings and predictive thermal management. 

Unlike affordable commercial alternatives, our design integrates a custom high-precision Monitoring Board. This allows for real-time tracking of input/output current, individual cell voltages, and precise battery temperatures to provide an accurate State of Charge (SoC) and true State of Health (SoH).

## Key Specifications
* **Battery Capacity:** 40Ah total capacity utilizing a 3S8P configuration of EVE 50E 21700 cells.
* **Power Outputs:** * 120W car lighter output (10A @ 12V)
  * 100W USB-C Power Delivery port
  * 65W USB-C standard port
  * Dual 12W USB-A ports
* **Safety:** Multi-layer protection combining a Daly 40A Battery Management System (BMS) with our custom monitoring board and active cooling.

## System Architecture
The system is controlled by an **STM32F103C8Tx Blue Pill** microcontroller. To simplify the design and communication path, all digital monitors, including the ADCs and the OLED screen, are hosted on a single I2C channel.

**Key Components:**
* **Current Monitoring:** INA series sensor (INA226/INA228) for highly accurate, bidirectional power measurement.
* **Voltage Sensing:** MCP6004 Op-Amp configured for true rail-to-rail I/O to safely step down the maximum cell voltage for the STM32's ADC.
* **Thermal Monitoring:** ADS1115 paired with NTC 10k thermistors to track cell temperatures across the pack.
* **User Interface:** An upgraded OLED display provides real-time feedback to the user on changing battery values.
* **Hardware Integration:** All power, sensing, and control components are being integrated into a custom-manufactured PCB.

## Team Members
* Gonzalo Cervera-Aguinaga
* Darren Mora
* Javier Castro-Frausto
* Andres Sierra Cantu

