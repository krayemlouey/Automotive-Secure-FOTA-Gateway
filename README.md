# 🛡️ Dual-Core Automotive Security Gateway & Real-Time TinyML CAN-IDS
### *Intelligent Intrusion Detection & Multi-Network Gateway for Connected Vehicles*

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: STM32H747](https://img.shields.io/badge/Platform-STM32H747I--DISCO-brightgreen.svg)](https://www.st.com/en/evaluation-tools/stm32h747i-disco.html)
[![Co-Processor: STM32F446](https://img.shields.io/badge/Bench-NUCLEO--F446RE-blue.svg)](https://www.st.com/en/evaluation-tools/nucleo-f446re.html)
[![AI Framework: STM32Cube.AI](https://img.shields.io/badge/AI-STM32Cube.AI%20%7C%20ONNX-orange.svg)](https://www.st.com/en/embedded-software/x-cube-ai.html)
[![GUI: TouchGFX](https://img.shields.io/badge/GUI-TouchGFX%20800x480-purple.svg)](https://support.touchgfx.com/)

---

## 📌 Executive Summary

Modern connected and autonomous vehicles integrate dozens of Electronic Control Units (ECUs) exchanging critical telemetry over Controller Area Networks (CAN / CAN-FD). However, the classical CAN protocol lacks native encryption, authentication, and origin verification, exposing vehicle architectures to malicious frame injection, Denial-of-Service (DoS), spoofing, and fuzzing attacks.

This project implements an **Intelligent Dual-Core Embedded Automotive Security Gateway & Intrusion Detection System (IDS)**. Rather than relying on cloud connectivity or heavy computational servers, the entire detection and mitigation pipeline is deployed **directly on-chip at the hardware edge** using the asymmetric **STM32H747 dual-core microcontroller** and **TinyML**.

```
                           VEHICLE CAN BUS INFRASTRUCTURE
                                         │
                 ┌───────────────────────┼───────────────────────┐
                 │                       │                       │
           [CAN Network 1]         [CAN Network 2]         [CAN Network 3]
           Chassis / Body           Powertrain              Infotainment
                 │                       │                       │
                 └───────────────────────┼───────────────────────┘
                                         ▼
                     ┌───────────────────────────────────────┐
                     │   STM32H747I-DISCO SECURITY GATEWAY   │
                     │                                       │
                     │  ┌─────────────────────────────────┐  │
                     │  │ Cortex-M4 Core (240 MHz)        │  │
                     │  │ • Real-Time CAN Frame Sniffer   │  │
                     │  │ • Feature Extraction Engine     │  │
                     │  │ • TinyML IDS Classifier (ONNX)  │  │
                     │  │ • Mitigation / Filtering Engine │  │
                     │  └────────────────┬────────────────┘  │
                     │                   │ IPC (SRAM4 + HSEM)│
                     │  ┌────────────────▼────────────────┐  │
                     │  │ Cortex-M7 Core (480 MHz)        │  │
                     │  │ • TouchGFX 800x480 Dashboard    │  │
                     │  │ • Diagnostics & Threat Alarms   │  │
                     │  │ • Gateway Supervisory Control   │  │
                     │  └─────────────────────────────────┘  │
                     └───────────────────────────────────────┘
                                         ▲
                                         │ Monitored Link
                     ┌───────────────────┴───────────────────┐
                     │ NUCLEO-F446RE BENCHMARK & ATTACK NODE │
                     │  • Manual Mode: Potentiometer Fuzzing │
                     │  • AI Mode: Synthetic Attack Bursts   │
                     └───────────────────────────────────────┘
```

---

## 🌟 Key Highlights & Innovations

1. **Dual-Core Asymmetric Isolation (AMP)**:
   - **Arm Cortex-M4 (240 MHz)** dedicated exclusively to hard real-time CAN reception (FDCAN), frame decoding, and TinyML inference (< 2 ms latency).
   - **Arm Cortex-M7 (480 MHz)** dedicated to supervision, supervisory state machine, and a rich **TouchGFX 800×480 GUI** with Chrom-ART DMA2D acceleration.
   - Guaranteed **Zero-Jitter**: Graphic rendering cannot stall or delay critical automotive safety packets.

2. **Edge TinyML Intrusion Detection System**:
   - Neural network trained on multi-attack automotive datasets (`dataset_can_ids.csv`).
   - Exported to ONNX and compiled into optimized C code (`can_ids_model.c` / `can_ids_model.h`) via **STM32Cube.AI**.
   - Quantized footprint: executes in **< 15 KB RAM** and **< 45 KB Flash**.

3. **Dual-Mode Benchmark & Stress Node (NUCLEO-F446RE)**:
   - **Manual Mode**: Physical potentiometer to vary bus load, message injection frequency, and trigger corner-case conditions.
   - **AI Stochastic Mode**: Autonomous attack burst generator mimicking real-world automotive threats:
     - *Stuff Error & CRC Injections*
     - *Denial-of-Service (DoS) high-priority floods*
     - *Targeted ECU ID Spoofing & Fuzzing*
     - *Bus-Off confinement triggering*

4. **Zero-Copy Inter-Core Communication (IPC)**:
   - Shared **SRAM4 memory mapped at `0x38000000`** in Domain D3.
   - Hardware Semaphores (**HSEM**) for conflict-free atomic state handoff.
   - MPU configured as *Non-Cacheable, Shareable* to prevent cache coherency hazards.

---

## 📂 Repository Structure

```
├── LOUEY/
│   └── F446RE_CAN_TX/                 # NUCLEO-F446RE Firmware
│       ├── Core/Src/
│       │   ├── main.c                 # Attack Generator State Machine & Potentiometer ADC
│       │   ├── tft_shield.c           # Local diagnostic status display driver
│       │   └── ...
│       └── Drivers/                   # STM32F4xx HAL & CMSIS Drivers
│
├── TinyML_CAN_IDS/                    # Machine Learning Pipeline
│   ├── train_and_export_cube_ai.py    # Training script & ONNX export
│   ├── dataset_can_ids.csv            # Labeled CAN traffic & anomaly dataset
│   ├── can_ids_model.onnx             # Exported ONNX neural network
│   ├── can_ids_model.c / .h           # Generated STM32Cube.AI inference code
│   ├── patch_cm4.py                   # Automated CM4 integration script
│   └── STM32CubeAI_Dashboard.html     # Interactive browser report & validation plots
│
├── TouchGFXProjects/
│   └── MyApplication/                 # STM32H747I-DISCO Dual-Core Project
│       ├── CM4/                       # Cortex-M4 Firmware (FDCAN + TinyML Engine)
│       ├── CM7/                       # Cortex-M7 Firmware (TouchGFX GUI & Supervisor)
│       │   └── TouchGFX/              # TouchGFX 800x480 UI Designs & Assets
│       ├── Common/                    # Shared data structures & memory definitions
│       ├── Drivers/                   # STM32H7xx HAL, BSP & CMSIS Drivers
│       └── Middlewares/               # TouchGFX Library & ST AI Runtimes
│
├── but .txt                           # Project Mission & Architecture Notes
├── .gitignore                         # Build artifact filter for clean commits
└── README.md                          # Project Documentation
```

---

## 🔬 TinyML Model Architecture & Performance

The embedded classifier analyzes temporal and behavioral sliding-window metrics calculated over CAN frame streams:

| Parameter | Specification |
|---|---|
| **Input Features** | Message Frequency, Inter-Arrival Delta ($\Delta t$), Payload Entropy, ID Variance, Error Counters |
| **Model Type** | Quantized Multi-Layer Perceptron (MLP) / 1D-CNN |
| **Target Classes** | `NORMAL_TRAFFIC`, `DOS_ATTACK`, `SPOOFING`, `FUZZING`, `INTEGRITY_VIOLATION` |
| **Inference Time** | **~1.15 ms** on Cortex-M4 @ 240 MHz |
| **Quantization** | INT8 post-training quantization |
| **Classification Accuracy** | **> 98.4%** across test splits |

---

## 🛠️ Hardware Setup & Wiring

| Interface | STM32H747I-DISCO | NUCLEO-F446RE | Notes |
|---|---|---|---|
| **CAN Bus (High/Low)** | FDCAN1 (via SN65HVD230) | CAN1 (via Transceiver) | 120 $\Omega$ termination at both ends |
| **Ground Reference** | GND | GND | Common reference plane |
| **Potentiometer Input** | — | Pin PA0 (ADC1_IN0) | 0.0V to 3.3V injection tuner |
| **Diagnostic Display** | Built-in 800×480 DSI/RGB | ST7735 / SPI TFT Shield | Real-time dual display |

---

## 🚀 How to Build & Run

### 1. Compile & Flash NUCLEO-F446RE (Fault Injector)
```bash
cd LOUEY/F446RE_CAN_TX
# Open in STM32CubeIDE and flash onto NUCLEO-F446RE
```

### 2. Train / Retrain the TinyML Model
```bash
cd TinyML_CAN_IDS
python -m pip install -r requirements.txt # numpy, pandas, scikit-learn, onnx
python train_and_export_cube_ai.py
```

### 3. Build & Flash STM32H747I-DISCO (Security Gateway)
1. Open `TouchGFXProjects/MyApplication/STM32CubeIDE` in **STM32CubeIDE**.
2. Build **Cortex-M4 project** (`CM4`) and flash into Bank 2 (`0x08100000`).
3. Build **Cortex-M7 project** (`CM7`) and flash into Bank 1 (`0x08000000`).
4. Program QSPI external Flash for TouchGFX graphic assets at `0x90000000`.

---

## 👨‍💻 Author

**Louey KRAYEM**  
*Embedded Systems & Edge-AI Engineer*  
- GitHub: [@krayemlouey](https://github.com/krayemlouey)  
- Contact: `loueykrayem@gmail.com`

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
