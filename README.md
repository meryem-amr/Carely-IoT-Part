# Carely 
**A smart baby monitoring and maternal wellness system.**

Carely helps new parents keep track of their baby's health and wellbeing, and
supports the mother during the postpartum period. It combines wearable sensors,
an IoT sound device, machine-learning models, and a mobile app in one connected system.

> This repository contains the **IoT part** of Carely, with a detailed look at the
> **biometric bracelet**.

---

## The Problem

New parents can't watch their baby around the clock. Sleep, breathing, body
temperature, and crying are hard to track by hand, and a missed warning sign can
be serious. Mothers also often neglect their own wellbeing while caring for a newborn.

## Our Solution

| Component | What it does |
|---|---|
| **Biometric bracelet** | Measures heart rate, body temperature, and movement, and detects apnea on-device |
| **IoT sound device** | Listens for crying and plays soothing lullabies |
| **ML models** | Cry detection (CNN) and sleep detection (XGBoost) |
| **Backend** | JWT authentication, alert logic, and vitals processing |
| **Mobile app (Flutter)** | Vitals monitoring, sleep tracking, vaccination management, meetings & feedback |


## IoT Overview

The IoT layer is built on **ESP32** microcontrollers. Each device publishes its
data to a **HiveMQ MQTT broker**, and the backend consumes it, runs the alert
logic and ML models, and serves the results to the mobile app.

| Device | Purpose |
|---|---|
| **Biometric Bracelet** | Heart rate, temperature, motion, apnea detection |
| **Sound Device** | Captures audio for cry detection and plays lullabies |

---

## Biometric Bracelet

A wearable ESP32 device that continuously monitors a baby's vital signs and
sends them to the backend over MQTT.

### Features

- **Heart rate** via MAX30102
- **Body temperature** via MAX30205
- **Motion tracking** via MPU6050 accelerometer
- **On-device apnea detection**
- **Raw accelerometer streaming** so the backend can compute sleep features
- **Sensor validity flags** so failed readings are never reported as real values

### Hardware

| Component | Role | Interface |
|---|---|---|
| ESP32 | Microcontroller + Wi-Fi | |
| MAX30102 | Heart rate | I²C |
| MAX30205 | Temperature | I²C |
| MPU6050 | Accelerometer | I²C |

### Wiring

All three sensors share one I²C bus.

| Signal | ESP32 Pin |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| VCC | 3.3 V |
| GND | GND |



### How It Works

1. Read all three sensors on a fixed schedule.
2. Validate the heart rate and temperature readings.
3. Run apnea detection locally on the motion data.
4. Publish vitals and raw X/Y/Z acceleration over MQTT.

**Apnea detection (on-device).** The bracelet raises an alert when **40
consecutive samples** fall below **0.05 m/s²**, sampled every **500 ms**. That is
about 20 seconds without meaningful movement.

**Backend alerts.** All other alert logic (high heart rate, fever, etc.) runs on
the backend, so thresholds can change without reflashing the device.

**Raw motion data.** The bracelet sends raw X/Y/Z acceleration. The backend
computes **ENMO** and **anglez** from it, which feed the sleep detection model.

