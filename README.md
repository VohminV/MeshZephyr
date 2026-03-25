# 📡 MeshZephyr --- Децентрализованная LoRa BBS Сеть

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-blue.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF-orange.svg)](https://docs.espressif.com/projects/esp-idf/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![LoRa](https://img.shields.io/badge/LoRa-868/915/433MHz-purple.svg)](https://lora-alliance.org/)

> **Возрождение классических BBS в эпоху LoRa mesh-сетей. Полностью
> автономная, децентрализованная система обмена сообщениями без
> интернета.**

------------------------------------------------------------------------

## 📖 О Проекте

**MeshZephyr** --- это современная реализация классической BBS (Bulletin
Board System) с использованием технологии LoRa для создания
децентрализованных mesh-сетей. Проект вдохновлён золотой эрой BBS 80-90х
годов и современными протоколами вроде FidoNet и Reticulum.

### 🎯 Основная Идея

Создать устойчивую сеть обмена сообщениями, которая: - Работает без
интернета и сотовой связи - Использует LoRa радиоканал - Поддерживает
mesh архитектуру - ANSI BBS интерфейс - Криптографическая защита
сообщений

------------------------------------------------------------------------

## 🌟 Возможности

| Функция          | Сервер | Клиент |
|------------------|:------:|:------:|
| NetMail          |   ✅   |   ✅   |
| Message Board    |   ✅   |   ✅   |
| File Transfer    |   ✅   |   ✅   |
| Chat             |   ✅   |   ✅   |
| Store & Forward  |   ✅   |   ⚠️   |
| Mesh Routing     |   ✅   |   ✅   |
| ANSI Terminal    |   ✅   |   ✅   |
| E2E Encryption   |   ✅   |   ✅   |
| SysOp Panel      |   ✅   |   ❌   |

------------------------------------------------------------------------

## 🏗️ Архитектура

Mesh сеть из узлов ESP32 + LoRa, где: - Server Node --- хранение почты и
файлов - Client Node --- пользователи - Авто‑маршрутизация - Store &
Forward

------------------------------------------------------------------------

## 🛠️ Требования к Железу

-   ESP32
-   SX1276 / SX1278
-   Антенна 868 / 915 / 433 MHz
-   Питание 5V / 3.3V

------------------------------------------------------------------------

## 📦 Установка

``` bash
git clone https://github.com/yourusername/MeshZephyr.git
cd MeshZephyr
```

Сборка:

``` bash
cd Server
pio run -t upload

cd Client
pio run -t upload
```

------------------------------------------------------------------------

## ⚙️ Конфигурация

Server config:

``` cpp
#define MY_NODE_ID 100
#define NODE_NAME "ESP32_BBS"
#define LORA_FREQ 868.0f
```

Client config:

``` cpp
#define MY_NODE_ID 101
#define SERVER_NODE_ID 100
#define NODE_NAME "CLIENT_01"
```

------------------------------------------------------------------------

## 📡 Протокол

Типы пакетов:

  Type   Название
  ------ ----------
  0x01   PING
  0x02   PONG
  0x03   BEACON
  0x04   CHAT
  0x05   MAIL
  0x06   FILE
  0x07   NODE

------------------------------------------------------------------------

## 🔐 Криптография

-   X25519 --- обмен ключами
-   Ed25519 --- подписи
-   AES‑128‑GCM --- шифрование
-   SHA‑256 --- хэш

------------------------------------------------------------------------

## 🗺️ Roadmap

-   Web интерфейс
-   Email gateway
-   SD card support
-   Voice messages
-   Bluetooth app
-   Auto routing
-   FEC

------------------------------------------------------------------------

## 📄 License

MIT License

------------------------------------------------------------------------

## MeshZephyr --- LoRa BBS нового поколения
