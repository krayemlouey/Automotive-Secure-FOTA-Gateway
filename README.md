# 🚗 Secure Automotive FOTA Gateway & Dual-Core Digital Cockpit
### *Compliant with UN R156 (Software Update Management Systems) & ISO/SAE 21434 (Automotive Cybersecurity)*

[![Platform: STM32H747](https://img.shields.io/badge/Target-STM32H747I--DISCO%20(Dual--Core)-brightgreen.svg)](https://www.st.com/en/evaluation-tools/stm32h747i-disco.html)
[![Gateway: ESP32](https://img.shields.io/badge/Telematics-ESP32%20DevKit%20V1-red.svg)](https://www.espressif.com/)
[![Co-Processor: STM32F446](https://img.shields.io/badge/ECU%20Bench-NUCLEO--F446RE-blue.svg)](https://www.st.com/en/evaluation-tools/nucleo-f446re.html)
[![GUI: TouchGFX](https://img.shields.io/badge/GUI-TouchGFX%2060FPS-purple.svg)](https://support.touchgfx.com/)
[![Standard: UN R156](https://img.shields.io/badge/Standard-UN%20R156%20%7C%20ISO%2021434-orange.svg)](https://unece.org/)

---

## 📌 Project Overview

In the era of the **Software-Defined Vehicle (SDV)**, deploying **Firmware Over-The-Air (FOTA)** updates is critical to eliminate costly physical workshop recalls and deliver instant functional improvements. 

However, deploying code remotely to an active vehicle involves strict safety and regulatory challenges:
1. **UN R156 (SUMS)**: Mandatory driver consent prior to installation, payload integrity verification, and zero disruption to active driving telemetry.
2. **UN R155 & ISO/SAE 21434**: End-to-end cybersecurity protection, out-of-band separation between untrusted wireless entrypoints and in-vehicle critical buses.

This repository hosts a production-grade **Hardware-in-the-Loop (HIL) Automotive FOTA Pipeline** connecting a Telematics Gateway, an ECU Simulator, and a Dual-Core High-Performance Instrument Cluster.

```
       [ MOBILE PHONE / TECHNICIAN ]
                     │ (Wi-Fi 802.11 / Web Portal)
                     ▼
  ┌─────────────────────────────────────┐
  │ 1. TELEMATICS GATEWAY (ESP32)       │
  │    • Embedded Web Server            │
  │    • Streaming CRC32 Engine         │
  │    • Out-of-Band CAN Signaling      │
  └──────────────────┬──────────────────┘
                     │ (UART 115200 bauds)
                     ▼
  ┌─────────────────────────────────────┐
  │ 2. VEHICLE GATEWAY (STM32F446RE)    │
  │    • Sensor Telemetry (ADC Speed)   │
  │    • Deterministic Command Routing  │
  └──────────────────┬──────────────────┘
                     │ (High-Speed CAN 500 kbps)
                     ▼
  ┌────────────────────────────────────────────────────────┐
  │ 3. TARGET DIGITAL COCKPIT (STM32H747I-DISCO)           │
  │    ├─ Cortex-M4 (240 MHz): Real-Time FDCAN1 & SRAM4    │
  │    └─ Cortex-M7 (480 MHz): TouchGFX 60 FPS Cluster     │
  │       • Driver Consent Modal ("MISE À JOUR DEMANDÉE")  │
  │       • Live Activation: v1.0.0 Eco ↔ v2.0.0 Sport     │
  │       • Non-Volatile RTC Backup Retention              │
  └────────────────────────────────────────────────────────┘
```

---

## ⚙️ Architecture & Hardware Roles

| Node | Microcontroller | Operating Role in the FOTA Pipeline |
|---|---|---|
| **Telematics Gateway (TCU)** | ESP32 DevKit V1 (240 MHz) | Hosts the secure Wi-Fi access point (`louey_ota`), receives the target binary (`update.bin` or `normal.bin`), calculates continuous **CRC32** on incoming data blocks, and transmits CAN control frames. |
| **Vehicle Gateway / Simulator** | STM32 Nucleo-F446RE (180 MHz) | Emulates live vehicle dynamics (potentiometer PA0 ADC for real-time speed, fuel, temperature) and bridges telematics commands to the CAN bus. |
| **Instrument Cluster ECU** | STM32H747I-DISCO (Dual-Core) | **Cortex-M4:** Real-time FDCAN reception into non-cacheable shared SRAM4 (`0x38000000`).<br/>**Cortex-M7:** TouchGFX GUI at 60 FPS, driver confirmation prompt, and live cluster morphing. |

---

## 📦 What is Being Updated ("The X")

The payload deployed over-the-air is a **monolithic binary image (~234 KB)** targeting the Cortex-M7 application bank:
* **ARM Vector Table & Reset Handlers**: Hardware exception routines and FreeRTOS task scheduler.
* **TouchGFX Rendering Engine**: Graphic dial shaders, anti-aliased needle interpolation, and font typography tables.
* **Dynamic Vehicle Calibration Maps**:
  * **v1.0.0 Standard Profile**: 200 km/h maximum scale, Cyan eco palette, Drive (D) mode.
  * **v2.0.0 Sport Profile**: 260 km/h extended scale, Racing Red palette, Sport (S) mode, and instant power metrics.
* **Integrity Validation**: Streaming CRC32 cross-checked against CAN ID `0x183` payload.

---

## 🛡️ UN R156 Driver Consent Workflow

To guarantee zero accidental reboots while operating the vehicle:

```
[ New Binary Uploaded ]
          │
          ▼
[ CAN ID 0x180 / 0x190 ]
          │
          ▼
[ Cluster shows non-intrusive alert: "MISE À JOUR DEMANDÉE" ]
          │
          ├────────────────────────┐
          ▼                        ▼
    [ Touch Banner ]         [ Ignore / Drive ]
          │                        │
          ▼                        ▼
 [ Modal Dialog Opens ]     (No interruption,
   • [ METTRE À JOUR ]       gauges continue 60 FPS)
   • [ PLUS TARD ]
          │
          ├────────────────────────┐
          ▼                        ▼
   [ PLUS TARD ]            [ METTRE À JOUR ]
          │                        │
          ▼                        ▼
(Dismisses modal,           (Animated install bar,
keeps current mode)          hot-morphs to Sport v2.0.0,
                             saves flag to RTC Backup Domain)
```

---

## 📡 CAN Bus Protocol Matrix (500 kbps)

| CAN ID | Direction | Frequency | Payload Description |
|---|---|---|---|
| **`0x100`** | Nucleo $\rightarrow$ H7 | 20 ms | Real-time vehicle speed (ADC 0..4095 mapped to 0..260 km/h). |
| **`0x101`** | Nucleo $\rightarrow$ H7 | 100 ms | Vehicle environment: Fuel (75%), Temperature (23°C), Humidity (42%). |
| **`0x102`** | Nucleo $\rightarrow$ H7 | 100 ms | ADAS Obstacle Distance (200 cm). |
| **`0x170`** | ESP32 $\rightarrow$ H7 | Event | High-priority Vehicle Alert (red banner, no update triggered). |
| **`0x180`** | ESP32 $\rightarrow$ H7 | Event | **OTA Update Announcement (v2.0.0 Sport)** $\rightarrow$ Triggers driver prompt. |
| **`0x181`** | ESP32 $\rightarrow$ H7 | Event | OTA Session Start Command. |
| **`0x183`** | ESP32 $\rightarrow$ H7 | Event | **Integrity Signature**: Bytes 0..3: CRC32 \| Bytes 4..7: Exact file size. |
| **`0x190`** | ESP32 $\rightarrow$ H7 | Event | **OTA Reset / Normal Announcement (v1.0.0 Eco)** $\rightarrow$ Triggers driver prompt. |

---

## 💾 State & Data Conservation Across Resets

Vehicle mileage (odometer) and current firmware mode must never be lost during power cycles or vehicle sleep states:
* **Hardware RTC Backup Domain (`0x58004050` - `RTC_BKP0R`)**: Powered by the backup power rail (`VBAT`).
* When v2.0.0 is confirmed, the magic value `0x55AA2000` is written to `RTC_BKP0R` and `SRAM4_FLAG` (`0x38000400`).
* Even upon pressing the physical **RESET** button, the cluster reads the backup register and instantly restores the Sport profile without requiring reflashing.

---

## 📑 Technical Documentation

A comprehensive, industry-grade technical dossier (in French) is available directly in this repository:
* 📄 **Download Dossier :** [DOCUMENTATION_TECHNIQUE_AUTOMOTIVE_SECURE_OTA.pdf](DOCUMENTATION_TECHNIQUE_AUTOMOTIVE_SECURE_OTA.pdf)

---

## 🚀 How to Run the Demo

1. **Hardware Setup**:
   * Connect ESP32 (SN65HVD230), Nucleo-F446RE, and STM32H747I-DISCO (CN2 PMOD PA11/PA12) to the common 500 kbps CAN bus with $120\,\Omega$ termination.
   * Connect ESP32 GPIO 17 (TX2) to Nucleo Pin D2 (PA10 RX1).
2. **Access Telematics Portal**:
   * Connect your smartphone/PC to Wi-Fi **`louey_ota`** (Password: `123456789`).
   * Navigate to `http://192.168.4.1`.
3. **Trigger Update**:
   * Choose `update.bin` (located on Desktop) and click **"Envoyer Mise a Jour (.bin)"**.
   * On the STM32H7 cluster, tap the top banner **"MISE À JOUR DEMANDÉE"**.
   * Tap **[METTRE À JOUR]** $\rightarrow$ Observe cluster transformation into the 260 km/h Sport Racing theme.
4. **Restore**:
   * Upload `normal.bin` to return to the v1.0.0 Eco profile.

---

## 👨‍💻 Author

**Louey KRAYEM**  
*Embedded Systems & Automotive Software Engineer*  
- GitHub: [@krayemlouey](https://github.com/krayemlouey)  
- LinkedIn: [Louey Krayem](https://linkedin.com)

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
