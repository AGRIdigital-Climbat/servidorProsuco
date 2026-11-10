# AgriAgents ESP32 Firmware v1.0.0

Firmware único, **sin OTA**, 100% configurable vía API, multi-modelo ESP32.

## 🎯 Filosofía

- **Un solo firmware** para todos los modelos ESP32
- **Cero OTA** → comunicación garantizada desde el primer boot
- **Configuración 100% remota** vía API (`GET /devices/{id}/config`)
- **Inteligencia en backend** (Hermes agents) → firmware "tonto" pero robusto
- **Anti-brick**: watchdog, auto-recovery, modo recovery por Serial

## 📦 Modelos Soportados (Auto-detección)

| Modelo | Placa | PSRAM | WiFi | BT | USB OTG | Thread/Zigbee |
|--------|-------|-------|------|----|---------|---------------|
| `esp32-devkit-v1` | ESP32-DevKitC (WROOM-32E/UE) | No | ✅ | ✅ | ❌ | ❌ |
| `esp32-wrover` | ESP32-WROVER-E/IE | 8MB | ✅ | ✅ | ❌ | ❌ |
| `esp32-s2` | ESP32-S2 DevKitC-1 / SAOLA-1 | No | ✅ | ❌ | ✅ | ❌ |
| `esp32-s3` | ESP32-S3 DevKitC-1 | Opcional | ✅ | ✅ | ✅ | ❌ |
| `esp32-c3` | ESP32-C3 DevKitC-02 | No | ✅ | 5.0 | ❌ | ❌ |
| `esp32-c6` | ESP32-C6 DevKitC-06 | No | 6 | 5.0 | ❌ | ✅ |
| `esp32-h2` | ESP32-H2 DevKitC-07 | No | ❌ | 5.0 | ❌ | ✅ |
| `esp32-p4` | ESP32-P4 DevKit | No | ❌ | ❌ | ❌ | ❌ |

**Cellular** (TinyGSM): SIM7000G, SIM7600, A7670, LilyGo T-SIM7000G, T-A7670E, T-SIM7600G

## 🏗 Arquitectura

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Firmware                           │
├─────────────────────────────────────────────────────────────┤
│  Network Manager  │  HTTP Client (HMAC)  │  Persistence    │
│  (WiFi/Cell/Eth)  │  (API v1)            │  (NVS + SD)     │
├─────────────────────────────────────────────────────────────┤
│  Sensor Manager   │  Command Processor   │  Watchdog       │
│  (I2C/ADC/Pulse)  │  (actuate/test)      │  (feed+delay1)  │
└─────────────────────────────────────────────────────────────┘
                              │
                    HTTPS + HMAC-SHA256
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    AgriAgents API (Backend)                 │
│  Config → Device → Sensors → Variables → Tasks → Commands  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Hermes Agents (Backend)                  │
│  control, monitor, logger, scheduler, learning, etc.        │
└─────────────────────────────────────────────────────────────┘
```

## ⚙️ Configuración Remota (API)

El firmware **NO tiene configuración hardcodeada**. Todo viene de `GET /v1/devices/{id}/config`:

```json
{
  "config_version": 5,
  "tz_offset_minutes": -240,
  "pins": {
    "i2c0_sda": 21, "i2c0_scl": 22,
    "i2c1_sda": 33, "i2c1_scl": 32,
    "adc1_pins": [32, 33, 34, 35, 36, 39],
    "dac_pins": [25, 26],
    "led_builtin": 2
  },
  "sensors": [
    {"sensor_id": "bme280_1", "kind": "env", "port": "i2c:0x76", "enabled": true, "interval_seconds": 60},
    {"sensor_id": "soil_1", "kind": "soil", "port": "adc:32", "enabled": true, "interval_seconds": 300},
    {"sensor_id": "flow_1", "kind": "flow", "port": "pulse:27", "enabled": true, "interval_seconds": 10}
  ],
  "variables": [
    {"key": "temperature", "enabled": true, "range_min": -40, "range_max": 80, "warn_min": -10, "warn_max": 50, "error_value": -999},
    {"key": "humidity", "enabled": true, "range_min": 0, "range_max": 100, "warn_min": 10, "warn_max": 90, "error_value": -999},
    {"key": "soil_moisture", "enabled": true, "range_min": 0, "range_max": 100, "error_value": -999}
  ]
}
```

## 📡 Protocolos de Comunicación

### 1. Ingesta de Lecturas (Batch)
```
POST /v1/devices/{id}/readings
HMAC: X-Device-Id, X-Timestamp, X-Signature
Body: {"seq": 123, "readings": [{"key": "temperature", "value": 23.5, "ts": "2026-01-15T10:00:00Z"}]}
```

### 2. Tareas Asíncronas (Análisis)
```
POST /v1/devices/{id}/tasks {"kind": "irrigation_analysis"}
→ 202 {tracking_id, poll_after_s}
GET /v1/tasks/{tracking_id} → 200 {command: {action: "irrigate", pin: "bomba", value: 1, duration_ms: 30000}}
POST /v1/tasks/{tracking_id}/ack {"result": "executed", "seconds_applied": 30}
```

### 3. Carril Rápido (Comandos Inmediatos)
```
GET /v1/commands/next?wait=25 → 200 {command_id, kind: "actuate", command: {action: "on", pin: "bomba"}}
POST /v1/commands/{id}/ack {"success": true}
```

### 4. Comandos Especiales
- `restart` → Reinicio remoto
- `pin_test` → Test pin individual (read/write/adc/pwm/i2c_scan)
- `self_test` → Test completo de pines

## 🔐 Autenticación HMAC-SHA256

```python
# Firmar request
payload = device_id + "." + timestamp + "." + sha256(body)
signature = hmac_sha256(hmac_secret, payload)
headers = {
    "X-Device-Id": device_id,
    "X-Timestamp": str(timestamp),
    "X-Signature": signature.hex()
}
```

## 💾 Persistencia

| Dato | Almacenamiento | Estrategia |
|------|----------------|------------|
| Config, HMAC, WiFi creds | NVS (Preferences) | Solo si cambió (`putIfChanged`) |
| Contadores (seq, uptime) | NVS (RAM + flush cada N) | Batch writes |
| Buffer lecturas offline | SD > SPIFFS > NVS (30 items) | Archivos binarios rotativos |
| Logs | SD/SPIFFS | Archivos de texto rotativos |

## 🐕 Watchdog

```cpp
// En loop() - CRÍTICO
g_wdt.feed();
delay(1);  // OBLIGATORIO después de cada feed
```

Timeout: 8s por defecto.

## 🔌 I2C Multi-Dispositivo

- 2 buses hardware (I2C0, I2C1) en paralelo
- Auto-detección de sensores conocidos (BME280, SHT31, ADS1115, MCP9808, etc.)
- Mux TCA9548A para colisiones de dirección
- Escaneo periódico cada 60s

## 📊 Sensores Soportados

| Tipo | Puerto | Drivers |
|------|--------|---------|
| Temp/Hum/Presión | `i2c:0x76` | BME280, BMP280 |
| Temp/Hum | `i2c:0x44` | SHT31, SHT40 |
| ADC 16-bit 4ch | `i2c:0x48` | ADS1115, ADS1015 |
| Temp alta prec. | `i2c:0x18` | MCP9808 |
| Luz | `i2c:0x23` | BH1750, TSL2561 |
| Analógico | `adc:1` | Cualquier sensor 0-3.3V |
| Caudalímetro | `pulse:27` | YF-S201, etc. |
| Contacto/Estado | `gpio:32` | Digital input |

## 🚀 Build & Flash

```bash
# Instalar PlatformIO
pip install platformio

# Build auto-detect (recomendado)
cd firmware/esp32_agri
pio run -e auto

# O modelo específico
pio run -e esp32-devkit-v1
pio run -e esp32-s3
pio run -e lilygo-t-sim7000g

# Flash
pio run -e auto -t upload

# Monitor
pio device monitor -b 115200
```

### Particiones (SIN OTA)

```ini
# platformio.ini
board_build.partitions = partitions_custom.csv
```

```csv
# partitions_custom.csv
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x100000,  # 1MB app
spiffs,   data, spiffs,  0x110000, 0xEF0000,  # ~14MB SPIFFS
```

## 📁 Estructura del Proyecto

```
firmware/esp32_agri/
├── platformio.ini
├── partitions_custom.csv
├── src/
│   ├── esp32_agri.ino          # Main entry point
│   ├── wifi_manager.cpp
│   ├── network_manager.cpp
│   ├── http_client.cpp
│   ├── sensor_manager.cpp
│   ├── command_processor.cpp
│   ├── persistence.cpp
│   └── watchdog.cpp
├── include/
│   ├── config.h
│   ├── board_config.h          # 9 modelos ESP32
│   ├── wifi_manager.h
│   ├── network_manager.h
│   ├── http_client.h
│   ├── sensor_manager.h
│   ├── command_processor.h
│   ├── persistence.h
│   └── watchdog.h
└── data/                       # Archivos para SPIFFS (certificados, etc.)
```

## 🔧 Configuración de Red

### WiFi (Portal cautivo si no hay credenciales)
1. Dispositivo inicia AP `AgriAgents-Setup` / `agriagents123`
2. Conectar a `http://192.168.4.1`
3. Escanear/seleccionar red → guardar
4. Reinicio automático

### Cellular (TinyGSM)
```cpp
// platformio.ini
build_flags = -D TINY_GSM_MODEM_SIM7000
// o SIM7600, A7670
```

Pines modem (configurables en API):
```json
{"cellular": {"tx_pin": 17, "rx_pin": 16, "power_pin": 4, "reset_pin": 5, "apn": "internet"}}
```

## 🔄 Flujo de Inicio

```
Boot
  │
  ▼
Detectar placa (ESP.getChipModel)
  │
  ▼
Inicializar Watchdog (PRIMERO)
  │
  ▼
Cargar config NVS/SD
  │
  ▼
Conectar WiFi/Cellular
  │
  ▼
Sincronizar hora (NTP)
  │
  ▼
Fetch config remota (GET /config)
  │
  ▼
Inicializar sensores (I2C scan + drivers)
  │
  ▼
Reportar sensores detectados a API
  │
  ▼
Solicitar tarea inicial
  │
  ▼
Loop principal (lecturas, tareas, comandos, watchdog)
```

## 🛡️ Seguridad

- **HMAC-SHA256** en todas las requests del dispositivo
- **HTTPS** obligatorio en producción
- **Certificados CA** en SPIFFS (`data/ca_cert.pem`)
- **Rotación de clave HMAC** vía API (`POST /devices/{id}/rotate-key`)
- **Sin OTA** = superficie de ataque mínima

## 📝 Logs y Debug

```cpp
// Niveles: debug, info, warning, error, critical
g_persistence.log("info", "Sensor conectado", "BME280");
g_persistence.log("error", "Lectura falló", "I2C_TIMEOUT");
```

Logs en:
- Serial (115200 baud)
- SD: `/logs.txt` (rotación 1MB)
- SPIFFS: `/logs.txt`

## 🧪 Testing

```bash
# Unit tests (host)
pio test -e native

# Simulador en backend
cd ../../backend
.venv/bin/python ../sim/device_sim.py --scenario dia-normal
```

## 📋 Checklist Producción

- [ ] Certificados CA en `data/ca_cert.pem`
- [ ] `platformio.ini` → `env:production` (CORE_DEBUG_LEVEL=0, -O2)
- [ ] Particiones custom (`partitions_custom.csv`)
- [ ] Clave HMAC única por dispositivo (generada al registrar)
- [ ] Timezone correcto en config (`tz_offset_minutes`)
- [ ] Watchdog timeout apropiado (8-15s)
- [ ] Buffer offline en SD si crítico
- [ ] Portal cautivo probado

## 📄 Licencia

MIT - Ver `LICENSE` en raíz del proyecto.

---

**AgriAgents** - Microcontroladores + Agentes = Control del mundo exterior
