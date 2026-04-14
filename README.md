Smart Plant Guardian

Full-Stack IoT Monitoring System (ESP32 + MQTT + PostgreSQL + Django)

📌 Overview

Smart Plant Guardian is a production-style IoT system that monitors environmental conditions in real time using an ESP32 device and delivers insights through a web dashboard.

This project was built to demonstrate end-to-end system design, combining:

Embedded systems (ESP32, sensors)
Real-time data streaming (MQTT)
Backend processing (Python)
Database engineering (PostgreSQL)
Web development (Django)
⚡ Features
📡 Real-time sensor data collection (temperature, humidity, pressure, soil moisture, light)
🔄 MQTT-based data streaming pipeline
🗄️ Structured PostgreSQL database with relational design
📊 Live dashboard with dynamic updates
🚨 Alert system for abnormal plant conditions
🧩 Modular and scalable architecture


🏗️ System Architecture
ESP32 (Sensors)-> MQTT Broker -> Python Backend (subscriber) -> PostgreSQL Database -> Django Web Dashboard

What This Project Demonstrates

Embedded Systems
ESP32 firmware using ESP-IDF
Sensor integration (I2C + ADC)
Structured JSON telemetry
🔹 Backend Engineering
MQTT client using Python
Data parsing and validation
Database logging pipeline
🔹 Database Design
Relational schema (plants, devices, sensor_readings, alerts)
Indexed queries for performance
Time-series data handling
🔹 Web Development
Django-based dashboard
REST API endpoints for sensor data
Dynamic frontend updates (JavaScript)
🗄️ Database Schema (Simplified)
plants
devices
sensor_readings
alerts

Author
Nana Owusu Abebrese
Computer Science Student | IoT & Systems Engineering Enthusiast
