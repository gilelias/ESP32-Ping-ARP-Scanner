# ESP32 Network Scanner with Parallel Tasking

## Overview
This project is an **ESP32-based network scanner** that detects active devices in a subnet using **ICMP (Ping) and ARP requests**.  
It leverages **FreeRTOS tasks** to **scan multiple IP ranges in parallel**, improving efficiency and speed.

## Features
- Scans the **entire subnet** dynamically based on the **subnet mask**.
- Uses **multiple FreeRTOS tasks** to perform parallel network scanning.
- **Extracts MAC addresses** using ARP requests.
- **Stores discovered devices** in a linked list with mutex protection.
- **Prints the discovered devices periodically**.

## Hardware Requirements
- ESP32 development board
- WiFi network

## Software Requirements
- **Arduino IDE** (or **PlatformIO**)
- ESP32 board package
- Required libraries:
  - `WiFi.h`
  - `ESP32Ping.h`
  - `lwip/etharp.h`
  - `FreeRTOS`

## How It Works
1. The ESP32 connects to the WiFi network.
2. The program calculates the total number of **usable IPs** in the subnet.
3. It splits the IP range among **multiple FreeRTOS tasks**.
4. Each task scans its assigned range by:
   - **Pinging each IP** to check if it's active.
   - If active, sending an **ARP request** to get its **MAC address**.
   - Storing the device info in a **linked list** (protected by a mutex).
5. Every **30 seconds**, the detected devices are printed.

## Code Structure
- `setup()`: Initializes WiFi, FreeRTOS tasks, and the network interface.
- `startParallelScan()`: Divides the IP range and spawns scan tasks.
- `scanTask()`: Scans a specific range using **Ping + ARP**.
- `pushDevice()`: Adds a device to the linked list.
- `printDevices()`: Prints the discovered devices.

## Usage
1. **Clone or download** this repository.
2. Open the project in **Arduino IDE**.
3. Modify `ssid` and `password`.
4. Upload the code to your ESP32.
5. Open the **Serial Monitor** (`115200 baud`) to view the scanning results.

## Example Output
```
Connected to WiFi 
IP Address: 192.168.1.100
Scanning subnet: 192.168.1.100/24 
Total Usable IPs: 254, Task Split: 63 IPs per task

Device Found: 192.168.1.2 
Device IP: 192.168.1.2, MAC: AA:BB:CC:DD:EE:FF 
Device Found: 192.168.1.3 
Device IP: 192.168.1.3, MAC: 1:22:33:44:55:66
```

## License
This project is open-source and licensed under the **MIT License**.
