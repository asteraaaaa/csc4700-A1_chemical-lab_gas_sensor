# 🧪 IoT-Based Gas Detection & Alert System

**CSC4700 – Embedded System Development (Assignment 1)**
Universiti Putra Malaysia (UPM)

## 📌 Project Overview

This project presents a **prototype IoT-based gas detection and alert system** designed for **chemistry laboratories, chemical storage rooms, and small warehouse environments**. The system continuously monitors air quality, detects abnormal gas conditions, triggers **local alerts**, and provides **remote monitoring and notifications** through a cloud dashboard.

The prototype demonstrates how **low-cost embedded systems and IoT platforms** can enhance safety by enabling early detection of hazardous conditions and reducing reliance on manual inspections.

---

## 🎯 Objectives

* Monitor laboratory air quality using an embedded gas sensor
* Demonstrate **multi-gas monitoring logic** using software-simulated gas values
* Trigger **local audible alerts** when unsafe conditions persist
* Enable **remote real-time monitoring** via Blynk IoT platform
* Log sensor data for historical analysis
* Design a **low-cost, scalable IoT safety prototype**

---

## 🧠 System Architecture (Summary)

**Edge-based IoT architecture** with cloud integration:

* **Edge Node**: ESP32 microcontroller
* **Sensing**: MQ-135 gas sensor (general air quality)
* **Actuation**: Piezo buzzer for local alerts
* **Cloud Platform**: Blynk IoT
* **Connectivity**: Wi-Fi (IEEE 802.11)
* **Monitoring**: Mobile/Web dashboard

> ⚠️ Note:
> Only general air quality is physically sensed using MQ-135. Individual gas concentrations (CO, CH₄, NH₃, H₂S, NO₂) are **software-simulated** to demonstrate multi-gas detection logic.

---

## 🔧 Hardware Components

| Component                 | Description                     |
| ------------------------- | ------------------------------- |
| ESP32 Dev Module          | Main controller and IoT gateway |
| MQ-135 Gas Sensor         | General air quality sensing     |
| Piezo Buzzer              | Local audible alert             |
| Breadboard & Jumper Wires | Prototyping                     |
| USB Power Supply          | Power source                    |

---

## 💻 Software & Tools

* **Arduino IDE**
* **C/C++ (ESP32 firmware)**
* **Blynk IoT Platform**
* **Wi-Fi Networking**
* **Threshold-based detection logic**

---

## 🔄 System Operation

1. MQ-135 samples air quality every **5 seconds**
2. ESP32 establishes a **baseline air quality profile**
3. New readings are compared against the baseline
4. **Sustained abnormal readings** trigger:

   * Local buzzer alert
   * Cloud notification via Blynk
5. Sensor data and alert status are logged to the cloud
6. Users monitor and control the system via Blynk dashboard

---

## 📊 Features

* ✅ Continuous air quality monitoring
* ✅ Sustained anomaly detection (reduces false alarms)
* ✅ Local audible alerts
* ✅ Remote monitoring & notifications
* ✅ Manual and automatic operation modes
* ✅ Cloud-based data logging

---

## 📈 Results Summary

* Alert triggered within **~5 seconds** after sustained abnormal readings
* Reliable Wi-Fi data transmission to Blynk cloud
* Stable operation during testing
* Successful recovery from short Wi-Fi disconnections

> This prototype validates the feasibility of IoT-based laboratory gas monitoring using affordable hardware.

---

## ⚠️ Limitations

* MQ-135 cannot distinguish specific gas types
* Multiple gas readings are **software-simulated**
* Prototype is **not a certified safety device**
* Depends on Wi-Fi availability

---

## 🚀 Future Improvements

* Integrate dedicated gas sensors (MQ-7, MQ-4, MQ-136, etc.)
* Perform real gas calibration and laboratory testing
* Add battery backup and offline buffering
* Support MQTT-based communication
* Deploy multiple sensor nodes

---

## 📁 Repository Structure

```
├── Gas-sensor.ino        # ESP32 firmware
├── README.md             # Project documentation
```

---

## 👥 Team Members (Group 4)

* Adibah Nawal Binti Muhammad Lukman
* Filzah Irdina Binti Ramdan
* Nurul Izzah Mardhiyyah Binti Mahmud
* Sharifah Hana Maisarah Binti Syed Omar
* Fatin Nurmaryam Binti Khairul Arifin
* Nur Farzana Alya Binti Saharudin

---

## 🏫 Course Information

* **Course**: Embedded System Development (CSC4700)
* **Lecturer**: Ts. Dr. Raihani Binti Mohamed
* **Institution**: Universiti Putra Malaysia

---

## 📜 License

This project is developed **for academic purposes only**.

