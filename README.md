# Picko — Mobile Robot Arm

Picko is a mobile robot arm that simulates an industrial pick-and-place system. It navigates autonomously between factory stations using IR sensors, picks up objects, and places them at target locations.

---

## Demo

> *Add photos or a video of Picko here*

---

## Concept

Picko mimics a real-world factory material handling system:

1. Picks up items from the **Warehouse**
2. Delivers them to the **Production Line**
3. Moves to the **Packaging** area for final placement

---

## Factory Map

```
[Production] -------- [Warehouse]
     |                     |
     |                     |
[Packaging]  -------- [Home]
```

Picko always travels in one direction around the loop:
**Home → Warehouse → Production → Packaging → Home**

---

## Hardware

| Component | Qty | Purpose |
|-----------|-----|---------|
| ESP32 Dev Board | 1 | Main controller + WiFi web server |
| L298N Motor Driver | 1 | Controls DC motors |
| TT DC Motors + Wheels | 4 | Movement |
| MG90S Servo | 4 | Robot arm joints |
| IR Sensor (TCRT5000) | 3 | Line following + station detection |
| Battery 7.4V (2S LiPo) | 1 | Main power supply |

---

## 3D Printed Arm

The robot arm is based on a 3D printed design.

- **Model:** [Robotic Arm with Base for Components](https://www.printables.com/model/1596575-robotic-arm-with-a-base-for-components)
- **Material:** PLA
- **Infill:** 20%
- **Layer height:** 0.2mm

---

## Wiring

### L298N Motor Driver → ESP32

| L298N Pin | ESP32 Pin |
|-----------|-----------|
| IN1 | 26 |
| IN2 | 27 |
| ENA | 32 |
| IN3 | 14 |
| IN4 | 12 |
| ENB | 33 |

### Servos → ESP32

| Servo | Joint | ESP32 Pin |
|-------|-------|-----------|
| S1 | Base | 13 |
| S2 | Shoulder | 15 |
| S3 | Elbow | 2 |
| S4 | Gripper | 4 |

### IR Sensors → ESP32

| Sensor | Position | ESP32 Pin |
|--------|----------|-----------|
| IR_L | Left | 34 |
| IR_M | Middle | 35 |
| IR_R | Right | 36 |

> Pins 34, 35, 36 are input-only on ESP32 — suitable for sensors.

---

## IR Station Detection

Picko uses 3 IR sensors (Left, Middle, Right) to identify its current station:

| L | M | R | Station |
|---|---|---|---------|
| 0 | 1 | 0 | On the line — keep moving |
| 1 | 1 | 1 | Production |
| 0 | 0 | 0 | Warehouse |
| 1 | 0 | 1 | Packaging |
| 0 | 0 | 1 | Home |

> `0` = white surface, `1` = black line detected

> **Note on localization:** IR pattern detection is one approach to knowing the robot's position. Other methods include wheel encoders for odometry, camera-based markers (QR codes), or IMU-based dead reckoning — each with different trade-offs in accuracy and complexity.

---

## Web Interface

Connect to: **WiFi: Picko / Password: 12345678**

Open browser at: `http://192.168.4.1`

### Pages

| Page | Description |
|------|-------------|
| Home | Choose between Manual and Auto mode |
| Manual | D-pad car control, arm sliders, preset positions |
| Auto | Factory map with station buttons and live location |

### Screenshots

> *Add screenshots of the web interface here*

---

## Calibration

Tune these values in the code after testing on the actual surface:

| Define | Default | Description |
|--------|---------|-------------|
| `TURN_TIME` | 600 ms | Duration of a 90° turn — increase if under-turning |
| `NAV_SPEED` | 180 | Line following speed (0–255) |
| `TURN_SPEED` | 160 | Turning speed — keep lower than NAV_SPEED |

### Arm Presets

Adjust these angles after assembling the arm:

| Preset | S1 Base | S2 Shoulder | S3 Elbow | S4 Gripper |
|--------|---------|-------------|----------|------------|
| Home | 90° | 90° | 90° | 90° |
| Pick | 90° | 45° | 135° | 10° |
| Drop | 180° | 90° | 90° | 170° |

---

## How to Run

1. Install libraries in Arduino IDE:
   - `ESP32Servo`
   - `WiFi` and `WebServer` (included with ESP32 core)

2. Open `picko_full.ino` in Arduino IDE

3. Select board: **ESP32 Dev Module**

4. Upload to ESP32

5. Connect your phone to WiFi **Picko**

6. Open `http://192.168.4.1` in your browser

---

## Repository Structure

```
Picko/
├── README.md
├── code.io
└── assets/
    ├── factory_map.png
    ├── ui_manual.png
    └──ui_auto.png 
```

---

## Future Improvements

- [ ] Add sorting method for object classification at packaging station
- [ ] Implement wheel encoders for more accurate navigation
- [ ] Fine-tune arm positions with inverse kinematics
- [ ] Add camera-based station detection

---

## Team

IEEE Menoufia Student Branch — 2026
Robotics & Automation Society (RAS) 

