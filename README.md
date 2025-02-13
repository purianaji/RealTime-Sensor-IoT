# RealTime-Sensor-IoT  

This repository contains the firmware for a **single-node IoT sensor system** designed for **Parkinson’s Disease monitoring**.  
It achieves precise synchronization (**121µs**) using **MQTT-SN** and **NTP**, ensuring **reliable real-time data transmission**.

---

## 🚀 Features
✔️ **Ultra-Low Latency Synchronization**: Achieves **121µs** precision using NTP.  
✔️ **Reliable Wireless Communication**: Uses **MQTT-SN (QoS 1)** for **lossless** message delivery.  
✔️ **Energy Efficient**: Optimized low-power modes for extended sensor operation.  
✔️ **Scalable & Adaptable**: Can be expanded into **multi-node** networks.  

---

## 🛠 Firmware Overview
- **Platform:** Raspberry Pi Pico W  
- **Programming Language:** C++  
- **Protocols Used:** MQTT-SN, NTP  
- **Core Functionalities:**  
  - Real-time synchronization of sensor data  
  - Efficient wireless data transmission  
  - Buffering & watchdog mechanisms for reliability  

---

## 🔧 Installation Guide

### 🏷️ **1️⃣ Clone this repository to your computer:**
```sh
git clone https://github.com/purianaji/RealTime-Sensor-IoT.git
```
### 🏷️ 2️⃣ Install Required Tools:
🔹 Arduino IDE or VS Code (with PlatformIO)
🔹 Ensure MQTT broker and NTP server are properly configured
### 🏷️ 3️⃣ Flash the Firmware:
🔹 Open the .ino file in Arduino IDE or VS Code
🔹 Upload the firmware to Raspberry Pi Pico W
### 🏷️ 4️⃣ Run & Monitor the System:
🔹 Configure MQTT & NTP settings
🔹 Run the system and monitor real-time data transmission


---

## 📜 License
This project is licensed under the MIT License – feel free to use, modify, and distribute.

---

## 🤝 Contributing
Want to improve this project? Feel free to open an Issue or submit a Pull Request.

---


## 📧 Contact
👤 Pooria Naji
✉️ puria.naji@gmail.com
🔗 [LinkedIn Profile](https://www.linkedin.com/in/pooria-naji-zavar-garmroudi-614284179/)


