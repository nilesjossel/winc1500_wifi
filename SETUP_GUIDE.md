# Quick Setup Guide - ATWINC1500 Mesh Network on Pico 2W

## Overview

This guide will help you quickly set up a mesh network using ATWINC1500 WiFi modules connected to Raspberry Pi Pico 2W boards.

## What You Need

### Hardware (per node)
- 1x Raspberry Pi Pico 2W (RP2350)
- 1x ATWINC1500 or ATWINC1510 WiFi module
- Jumper wires
- Breadboard (optional)
- USB cable for programming/power

### Minimum Network
- At least 2 nodes for a basic mesh
- Recommended: 3-4 nodes for interesting mesh routing

## Wiring Diagram

Connect the ATWINC1500 to your Pico 2W:

```
ATWINC1500          Pico 2W (GPIO)
----------          --------------
SCK         <--->   GPIO 18 (SPI0 SCK)
MOSI        <--->   GPIO 19 (SPI0 TX)
MISO        <--->   GPIO 16 (SPI0 RX)
CS          <--->   GPIO 17 (SPI0 CS)
WAKE        <--->   GPIO 20
RESET       <--->   GPIO 21
IRQ         <--->   GPIO 22
VCC         <--->   3.3V
GND         <--->   GND
```

**Important Notes:**
- Ensure stable 3.3V power supply (ATWINC1500 can draw ~200mA)
- Keep wires short to minimize SPI interference
- Add decoupling capacitors (0.1µF) near ATWINC1500 VCC pin

## Software Setup

### 1. Install Prerequisites

```bash
# Install Pico SDK (if not already installed)
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable
export PICO_SDK_PATH=/path/to/pico-sdk
```

### 2. Update CMakeLists.txt

Edit `part2/CMakeLists.txt` and update the SDK path:

```cmake
set(PICO_SDK_PATH "/YOUR/PATH/TO/pico-sdk")
```

### 3. Configure Each Node

For each Pico 2W board, edit `part2/winc_pico_part2.c`:

**Node 1:**
```c
#define ENABLE_MESH_MODE 1
#define MESH_NODE_ID     1
#define MESH_NODE_NAME   "PicoNode1"
```

**Node 2:**
```c
#define ENABLE_MESH_MODE 1
#define MESH_NODE_ID     2
#define MESH_NODE_NAME   "PicoNode2"
```

**Node 3:**
```c
#define ENABLE_MESH_MODE 1
#define MESH_NODE_ID     3
#define MESH_NODE_NAME   "PicoNode3"
```

> **Critical:** Each node MUST have a unique `MESH_NODE_ID` (1-255)

### 4. Build the Firmware

```bash
cd part2
mkdir build
cd build
cmake ..
make
```

This creates `winc_wifi.uf2`

### 5. Flash Each Node

For each Pico 2W:

1. Hold BOOTSEL button while connecting USB
2. Pico appears as USB mass storage device
3. Copy `winc_wifi.uf2` to the drive
4. Pico automatically reboots and runs the firmware

## Testing Your Mesh Network

### 1. Connect Serial Console

Connect to each Pico's UART at 115200 baud:

```bash
# Linux/Mac
screen /dev/ttyACM0 115200

# Windows (use PuTTY, TeraTerm, or similar)
# COM port, 115200 baud, 8N1
```

### 2. Expected Output

On boot, you should see:

```
=====================================
  ATWINC1500 Mesh Network Example
  Raspberry Pi Pico 2W (RP2350)
=====================================

Node Configuration:
  ID: 1
  Name: PicoNode1
  P2P Channel: 1

Initializing SPI interface...
Disabling CRC...
Initializing ATWINC1500 chip...
Firmware X.X.X, OTP MAC address XX:XX:XX:XX:XX:XX

=== Mesh Networking Mode ===
Node ID: 1
Node Name: PicoNode1
===========================

Enabling P2P mode on channel 1
P2P mode enabled
Mesh networking enabled
Mesh socket X UDP port 1025 ok
Mesh socket X TCP port 1026 ok

Waiting for P2P connections...
```

### 3. Watch the Mesh Form

After ~30 seconds, you should see routing tables being built:

```
=== Mesh Routing Table ===
Local Node ID: 1 (PicoNode1)
Active Nodes: 2
Node ID  Hops  Next Hop  Active
-------  ----  --------  ------
    2     1       2       Yes
    3     2       2       Yes
========================
```

This shows:
- Node 1 can reach Node 2 directly (1 hop)
- Node 1 can reach Node 3 via Node 2 (2 hops)

## Troubleshooting

### Problem: "Can't initialise chip"

**Causes:**
- Wiring issue
- Power supply insufficient
- SPI speed too high

**Solutions:**
1. Double-check all connections
2. Use external 3.3V regulator if USB power is unstable
3. Try reducing `SPI_SPEED` in code (e.g., 8000000)

### Problem: P2P mode fails

**Causes:**
- ATWINC1500 firmware doesn't support P2P
- Hardware issue

**Solutions:**
1. Check ATWINC1500 firmware version (needs 19.x.x or later)
2. Verify chip initialization succeeds
3. Check IRQ line with multimeter/oscilloscope

### Problem: No nodes in routing table

**Causes:**
- Nodes on different P2P channels
- Nodes too far apart
- P2P connections not forming

**Solutions:**
1. Ensure all nodes use same `P2P_CHAN` (default: 1)
2. Move nodes closer together (< 10 meters initially)
3. Wait longer (beacons sent every 5 seconds)
4. Check verbose output for beacon messages

### Problem: Nodes disconnect randomly

**Causes:**
- Power supply issues
- Radio interference
- Timeout settings too aggressive

**Solutions:**
1. Use stable power supply
2. Move away from 2.4GHz interference sources
3. Increase `MESH_ROUTE_TIMEOUT` in `winc_p2p.h`

## Testing Mesh Communication

### Using Python Scripts

The project includes Python test scripts:

```bash
# On a computer connected to same network as one mesh node
python3 part2/udp_tx.py
```

This sends UDP packets to port 1025. If mesh is working:
1. Packet arrives at entry node
2. Gets routed through mesh
3. Echo comes back

### Manual Testing

Use netcat or similar tools:

```bash
# UDP test
echo "Hello mesh!" | nc -u <node-ip> 1025

# TCP test
echo "Hello mesh!" | nc <node-ip> 1026
```

## Network Topologies

### Linear Chain
```
Node1 --- Node2 --- Node3 --- Node4
```
- Simple to set up
- Tests multi-hop routing
- Vulnerable to single point failure

### Star
```
       Node2
         |
Node3--Node1--Node4
         |
       Node5
```
- Central hub (Node1)
- Good for testing central coordinator
- Hub is critical point

### Mesh
```
Node1 --- Node2
 |    \  /  |
 |     \/   |
 |     /\   |
 |    /  \  |
Node3 --- Node4
```
- Multiple paths between nodes
- Best redundancy
- Tests route selection

## Next Steps

### Experiment Ideas

1. **Range Testing:**
   - Gradually increase distance between nodes
   - Measure RSSI and packet loss

2. **Mesh Healing:**
   - Remove a node and watch mesh re-route
   - Add node back and watch it rejoin

3. **Data Applications:**
   - Send sensor data through mesh
   - Implement chat between nodes
   - Create distributed sensor network

4. **Performance Testing:**
   - Measure throughput vs. hop count
   - Test latency across different paths
   - Stress test with multiple simultaneous transfers

### Code Modifications

Try modifying these for learning:

1. **Beacon Interval** (`winc_p2p.h`):
   ```c
   #define MESH_BEACON_INTERVAL 5000  // Try 2000 or 10000
   ```

2. **Max Hops** (`winc_p2p.h`):
   ```c
   #define MESH_MAX_HOPS 4  // Try 2 or 8
   ```

3. **Route Timeout** (`winc_p2p.h`):
   ```c
   #define MESH_ROUTE_TIMEOUT 30000  // Try 15000 or 60000
   ```

## Advanced Configuration

### Using Different P2P Channels

If you have multiple mesh networks:

```c
// Network A - uses channel 1
#define P2P_LISTEN_CHAN P2P_CHAN_1

// Network B - uses channel 6
#define P2P_LISTEN_CHAN P2P_CHAN_6
```

### Verbose Output

Increase debugging output:

```c
#define VERBOSE 2  // 0=silent, 1=normal, 2=verbose, 3=very verbose
```

## Safety Notes

⚠️ **Important:**
- Don't exceed 3.3V on any pin
- Don't draw more than 300mA from Pico's 3.3V regulator
- Use external regulator for multiple peripherals
- Add ESD protection in production designs

## Getting Help

1. Check the detailed [README_MESH.md](part2/README_MESH.md)
2. Review ATWINC1500 documentation from Microchip
3. Examine reference blog posts from iosoft.blog
4. Use oscilloscope to debug SPI communication
5. Enable verbose output for detailed logs

## Summary Checklist

- [ ] Hardware assembled and wired correctly
- [ ] Pico SDK installed and path configured
- [ ] Each node has unique ID configured
- [ ] Firmware built successfully
- [ ] All nodes flashed with appropriate firmware
- [ ] Serial console connected at 115200 baud
- [ ] Chip initialization succeeds
- [ ] P2P mode enables successfully
- [ ] Beacons being sent (check verbose output)
- [ ] Routing tables populating
- [ ] Test communication working

Good luck with your mesh network! 🚀
