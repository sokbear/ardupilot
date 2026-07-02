# Стенд Pi 5: видео + трекинг метки + гимбал MG946R + PAL composite



Программа `electronic_pilot_bench` — **этап 1** стендового проекта (без SBUS, без RP2040):



1. камера на **двухосевом гимбале** (серво **MG946R**: pan/azimuth + tilt);

2. захват 1920×1080 (libcamera);

3. детект **тёмной метки** на белом листе (OpenCV);

4. **PID-слежение**: гимбал держит метку в центре кадра;

5. downscale → 720×576, OSD (рамка, ошибка ex/ey, углы pan/tilt);

6. вывод на аналоговый монитор (composite J7).



**Модель метки на стенде:** лист белой бумаги с нарисованной контрастной точкой/фигурой.  

Перемещайте лист — камера на гимбале должна поворачиваться, удерживая метку в поле зрения.



---



## Схема стенда



```

                    ┌─────────────┐

  лист с меткой ──► │  камера     │

                    │  на гимбале │

                    │ pan / tilt  │

                    └──────┬──────┘

                           │ CSI

                    ┌──────▼──────┐

                    │  Pi 5       │

                    │  detect+PID │──► I2C → PCA9685 → MG946R

                    │  OSD+PAL    │──► J7 → PAL-монитор

                    └─────────────┘

```



| Ось | Серво | Канал PCA9685 | Ошибка |

|-----|--------|---------------|--------|

| **Pan** (азимут) | MG946R | **0** | `ex` — метка левее/правее центра |

| **Tilt** (наклон) | MG946R | **1** | `ey` — метка выше/ниже центра |



### PCA9685 + MG946R



| PCA9685 | Pi 5 (40-pin) |

|---------|----------------|

| VCC | 3.3 V (логика) |

| GND | GND |

| SDA | GPIO 2 (pin 3) |

| SCL | GPIO 3 (pin 5) |

| V+ | **отдельный 5 V БП** для серв |

| GND (клемма питания) | общая земля с Pi и БП |



Сигналы: **PWM0** → pan, **PWM1** → tilt. Каналы и адрес I2C — в `config.h`.



В `/boot/firmware/config.txt`: `dtparam=i2c_arm=on`. Проверка: `sudo i2cdetect -y 1` (адрес **0x40**).



**Питание:** V+ PCA9685 — БП 5 V (1–2 A на два MG946R). Общая GND обязательна; не питать сервы только от 5 V pin Pi.



---



## Подключение PAL-монитора (J7)



На Pi 5 composite на **J7** у **HDMI0** (ближе к USB-C):



| Пад J7 | Форма | RCA |

|--------|--------|-----|

| GND | квадрат | корпус |

| CVBS | круг | центр |



Схемы и фото: `bench/images/` (`pi5-j7-official-photo.jpg`, `pi5-element14-pinout-full.png`).



Настройка PAL — см. `boot/pi5-pal-composite.txt` и раздел ниже.



---



## Настройка Pi 5 (PAL)



`/boot/firmware/config.txt`:



```ini

dtoverlay=vc4-kms-v3d,composite

```



`/boot/firmware/cmdline.txt` (в конец одной строки):



```

vc4.tv_norm=PAL video=Composite-1:720x576i,tv_mode=PAL

```



---



## Зависимости



```bash

sudo apt update

sudo apt install -y build-essential cmake pkg-config git \

  libcamera-dev libcamera-apps rpicam-apps \

  libopencv-dev libdrm-dev i2c-tools

```



---



## Сборка и запуск



```bash

cd ~/ardupilot/sbus_capture/bench

mkdir -p build && cd build

cmake ..

make -j4

./electronic_pilot_bench

```



Без доступа к `/dev/i2c-1` или PCA9685 программа запустится, но сервы не двигаются (режим `(sim)` на OSD).



Пользователь в группах `video`, `i2c` (для `/dev/i2c-1`):



```bash

sudo usermod -aG video,render,i2c $USER

```



---



## Калибровка метки



В `config.h`:



- `kThresholdValue` — порог (тёмная метка на белом: `kDetectDarkBlob = true`);

- `kMinBlobArea` / `kMaxBlobArea` — размер контура;

- `kPanPidKp`, `kTiltPidKp` — чувствительность слежения (начните с 0.02–0.06).



---



## Модули



| Файл | Назначение |

|------|------------|

| `camera_capture.*` | libcamera |

| `marker_detector.*` | blob на белом листе |

| `gimbal_tracker.*` | PID ex/ey → углы |

| `servo_gimbal.*` | MG946R через PCA9685 |
| `pca9685.*` | драйвер I2C PWM |

| `osd_renderer.*` | OSD на lores |

| `composite_out.*` | PAL composite |

| `config.h` | пороги, I2C/PCA9685, PID |



---



## Дальше (этап 2+)



- SBUS man-in-the-middle через RP2040;

- метка 1.5 m, дальность съёмки;

- связка с полётником ArduPilot.

