# STM32F103C8T6 — SSD1306 OLED Sensor Display

## Wiring

| OLED Pin | STM32 Pin |
|----------|-----------|
| VCC      | 3.3V      |
| GND      | GND       |
| SCL      | PB6       |
| SDA      | PB7       |

| Signal           | STM32 Pin |
|------------------|-----------|
| External Voltage | PA1       |

---

## What the screen shows

```
┌────────────────────────┐
│    STM32 Sensors       │
│ Temp: 27.4 C           │
│ V_ch1: 1.412 V         │
│ V_ch2: 1.650 V         │
└────────────────────────┘
```

- **Temp** — STM32 internal temperature sensor (~±5°C accuracy)
- **V_ch1** — raw voltage from the internal sensor
- **V_ch2** — voltage measured on PA1 (0 to 3.3V)

Refreshes every 200ms.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| Blank screen | Check SDA/SCL wiring on PB7/PB6 |
| Wrong temperature | Normal — internal sensor varies per chip |
| V_ch2 always 0 | Check your signal is connected to PA1 |

