# ESP32-S3 Local Access Point OTA Update System

A modular, lightweight, and robust Over-The-Air (OTA) firmware update system for the ESP32-S3 using ESP-IDF. This project configures the ESP32-S3 as a standalone Wi-Fi Access Point (SoftAP) hosting a native HTTP server. Firmware updates are securely streamed as raw binary files (`application/octet-stream`) directly from a mobile device or computer browser without requiring an internet connection or external cloud dependencies.

---

## 🧠 System Architecture & Concepts

### 1. Dual-Partition Ping-Pong Flash Setup
The system utilizes a custom partition layout designed for safe, fail-safe updates. It consists of a `factory` boot app partition and two identical OTA slots (`ota_0` and `ota_1`).

[ Flash Memory ]
├── factory (3MB)  --> Initial fallback firmware
├── ota_0   (3MB)  --> Active App Slot (Example)
└── ota_1   (3MB)  <-- Next Update targeted here (Passive Slot)

* **Zero-Downtime Verification:** When an update begins, the ESP32-S3 automatically identifies the inactive partition, erases its memory space, and streams the new binary directly into it.
* **Rollback Protection:** If the update process fails midway or the file is corrupted, the device remains safe because the current active partition is untouched. The `otadata` partition switches over only after successful binary validation.

### 2. High-Performance Binary Streaming
Instead of standard `multipart/form-data` uploads which inject heavy string parsing patterns (boundaries, metadata headers) into microcontrollers, the front-end application forces a raw **`application/octet-stream`** write hook. 
The first byte arriving at the network socket layer is natively the first byte of the executable app binary—allowing direct chunk passing straight into `esp_ota_write()`.

---

## 📂 Project Structure

```text
├── main/
│   ├── CMakeLists.txt      # Component build configuration
│   ├── main.c              # Application entry point & initialization 
│   ├── ota.c               # SoftAP Wi-Fi and HTTP server logic
│   └── ota.h               # Core OTA definitions & headers
├── partitions.csv          # Custom 16MB/8MB Flash layout allocation
├── index.html              # Standalone web app portal for file uploading
└── README.md               # Documentation
