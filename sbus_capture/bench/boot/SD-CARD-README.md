# Подготовка microSD для Pi 5 (стенд electronic_pilot_bench)

## Быстрый способ (рекомендуется)

1. SD-карта в ПК (сейчас диск **H:**).
2. **Правый клик** по `FLASH-SD-ADMIN.bat` → **Запуск от имени администратора**.
3. Подтвердить UAC.
4. Дождаться «Готово» (~5–10 мин: образ уже скачан в `%TEMP%\epilot-sd-prep`).

## Что записывается

- **Raspberry Pi OS Lite 64-bit** (Trixie, Pi 5)
- **SSH** включён
- Пользователь **`pilot`**, пароль **`epilot2026`**
- Hostname **`epilot-bench`**
- **I2C** (PCA9685), **PAL composite** (J7), **Camera Module 3**
- Пакеты для сборки bench (git, cmake, libcamera, opencv…)

## После первой загрузки Pi

```bash
ssh pilot@epilot-bench   # или ssh pilot@<IP>
passwd                  # сменить пароль

libcamera-hello -t 5000
sudo i2cdetect -y 1     # PCA9685 → 0x40

git clone <ваш-репозиторий> ~/ardupilot
cd ~/ardupilot/sbus_capture/bench && mkdir -p build && cd build
cmake .. && make -j4
./electronic_pilot_bench
```

## Альтернатива: Raspberry Pi Imager вручную

1. Imager → Raspberry Pi OS (other) → **Lite (64-bit)** → диск H:
2. Настройки (шестерёнка): hostname `epilot-bench`, user `pilot`, SSH, I2C.
3. После записи скопировать на boot-раздел из `sd-card/`:
   - `user-data`, `network-config`, `meta-data`
4. В `firmware/config.txt`: `dtoverlay=vc4-kms-v3d,composite`, `dtparam=i2c_arm=on`
5. В `firmware/cmdline.txt` в конец: `vc4.tv_norm=PAL video=Composite-1:720x576i,tv_mode=PAL`

## Файлы

| Файл | Назначение |
|------|------------|
| `prepare-sd-card.ps1` | Скачивание образа + прошивка + патч boot |
| `FLASH-SD-ADMIN.bat` | Запуск скрипта с правами администратора |
| `sd-card/user-data` | cloud-init: пользователь, SSH, пакеты |
| `sd-card/network-config` | Ethernet DHCP |
| `sd-card/config-firmware-append.txt` | composite + I2C |

**Внимание:** прошивка уничтожает все данные на карте (сейчас на H: — exFAT/GoPro).
