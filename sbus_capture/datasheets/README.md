# Даташиты компонентов «электронный пилот»

Каталог: `sbus_capture/datasheets/`

## Скачано (основная архитектура)

| Файл | Компонент | Роль в проекте |
|---|---|---|
| `ADV7280-M_datasheet.pdf` | Analog Devices ADV7280/7280-M | CVBS → MIPI CSI-2, I2P (основной видео-мост) |
| `ADV7282-M_datasheet.pdf` | Analog Devices ADV7282/7282-M | Альтернатива ADV7280-M |
| `MAX7456_datasheet.pdf` | Analog Devices MAX7456 | OSD (телеметрия + рамка метки) |
| `RP2040_datasheet.pdf` | Raspberry Pi RP2040 | SBUS MiTM + FSM + драйвер MAX7456 |
| `RP2040_hardware_design.pdf` | Raspberry Pi | Руководство по разводке платы с RP2040 |
| `THS7374_datasheet.pdf` | Texas Instruments THS7374 | Буферный сплиттер CVBS |
| `RaspberryPi_Zero2W_product_brief.pdf` | Raspberry Pi Zero 2 W (BCM2710A1 в RP3A0) | Вычислитель зрения |
| `TP9950_datasheet.pdf` | Techpoint TP9950 | Альтернатива ADV728x (HD-TVI/CVBS → MIPI) |

## Не скачано / недоступно публично

| Документ | Причина |
|---|---|
| `ADV728x_hardware_reference_UG-637.pdf` | Таймаут/блокировка analog.com с этой машины. Скачать вручную: [UG-637](https://www.analog.com/media/en/technical-documentation/user-guides/adv7280_7281_7282_7283_ug-637.pdf) |
| TP2860 | Нет публичного PDF; запрос у Techpoint (www.techpointinc.com) |
| BCM2710A1 | Полный даташит Broadcom не публикуется; см. product brief Pi Zero 2 W |

## Полётный контроллер, приёмник, VTX

Конкретная микросхема FC не зафиксирована (любой ArduPilot-совместимый FC, обычно STM32). TBS Nano и VTX — готовые модули, даташиты зависят от выбранной модели.

## Повторная загрузка

```bash
py download_datasheets.py
```

Скрипт скачивает PDF с официальных сайтов производителей.
