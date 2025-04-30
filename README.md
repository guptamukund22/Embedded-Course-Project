# Byzantine Fault Tolerance Consensus on STM32

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An implementation of the Byzantine Fault Tolerance (BFT) consensus algorithm on the STM32F429ZIT6 microcontroller. This project demonstrates that blockchain consensus algorithms can be successfully adapted for resource-constrained embedded systems.

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Demo](#demo)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [Implementation Details](#implementation-details)
- [Results and Performance](#results-and-performance)
- [Challenges and Solutions](#challenges-and-solutions)
- [Future Work](#future-work)
- [Contributors](#contributors)
- [References](#references)

## 🔍 Overview

This project investigates the application of blockchain consensus algorithms on embedded systems with limited resources, specifically using the STM32F429ZIT6 microcontroller. After evaluating various consensus methods, we determined that Byzantine Fault Tolerance (BFT) was the most appropriate choice for implementation on embedded devices due to its predictable behavior and reasonable resource demands compared to other options like Proof of Work and Proof of Stake.

## ✨ Features

- **Multi-Node Simulation**: Simulates four nodes on a single STM32F429ZIT6 board, with one node displaying Byzantine (faulty) behavior
- **Complete BFT Implementation**: Includes node state machines, circular buffer message queues, Byzantine fault simulation, and timeout mechanisms
- **Resource-Optimized Design**: Tailored for embedded systems with limited processing power, memory, and energy resources
- **Fault Tolerance**: Demonstrates tolerance of one Byzantine node in a four-node network, validating the theoretical n ≥ 3f + 1 requirement
- **Well-Documented Architecture**: Comprehensive documentation of the design choices, challenges, and solutions

## 🎬 Demo

Watch the demonstration video: [BFT on STM32 Demo](https://youtu.be/TRHFr6iX2Tk?feature=shared)

## 🛠 Requirements

### Hardware
- STM32F429ZIT6 Development Board
- ST-Link V2 programmer
- USB to UART converter (for debug output)

### Software
- Keil MDK (μVision 5)
- STM32CubeMX
- PuTTY or another serial terminal

## 🚀 Getting Started

### Setup
1. Clone this repository
   ```bash
   git clone https://github.com/yourusername/bft-stm32.git
   cd bft-stm32
   ```

2. Open the project in Keil μVision 5
   ```
   Open bft_consensus.uvprojx
   ```

3. Build the project and flash to your STM32F429ZIT6 board

4. Connect via UART to see the consensus progress
   - Baud rate: 115200
   - Data bits: 8
   - Stop bits: 1
   - Parity: None
   - Flow control: None

### Project Structure
```
bft-stm32/
├── Inc/
│   ├── main.h
│   ├── node.h
│   ├── message_buffer.h
│   ├── bft_protocol.h
│   └── stm32f4xx_hal_conf.h
├── Src/
│   ├── main.c
│   ├── node.c
│   ├── message_buffer.c
│   ├── bft_protocol.c
│   └── system_stm32f4xx.c
├── MDK-ARM/
│   └── bft_consensus.uvprojx
└── README.md
```

## 📝 Implementation Details

### Architecture

We simulated a distributed BFT network on a single STM32 board through a super-loop structure with circular buffer message passing. After evaluating several methods, including threading based on FreeRTOS, this approach proved most effective for adapting a distributed protocol to a single device.

### Core Components

1. **Node State Machine**
   - Each node progresses through BFT protocol states (INITIAL→PROPOSE→PREVOTE→PRECOMMIT→COMMIT→DECIDED)
   - State transitions follow strict consensus rules with supermajority requirements

2. **Message Passing System**
   - Circular buffers for inter-node communication
   - Fixed-size message arrays with read/write pointers
   - Buffer status tracking to prevent overflow
   - Round-robin message processing for fairness

3. **Byzantine Behavior Simulation**
   - One designated faulty node (Node 1) exhibits Byzantine behavior with 30% probability
   - Behaviors include: sending contradictory values, selectively ignoring messages, and committing incorrect values

4. **Timeout Mechanism**
   - Prevents protocol stagnation
   - Forces progress after a configurable number of cycles without state change

## 📊 Results and Performance

Our implementation successfully demonstrated consensus across multiple rounds despite Byzantine interference. The key results include:

- **Consensus Success**: All honest nodes consistently reached agreement across five rounds of testing
- **Byzantine Resilience**: The system correctly maintained consensus among honest nodes despite deliberate disruption
- **Resource Usage**: Our implementation fits comfortably within the STM32F429ZIT6's constraints
- **Performance**: Each consensus round typically required 3-4 state transitions per node

## 🧩 Challenges and Solutions

| Challenge | Solution |
|-----------|----------|
| Single-Device Simulation | Implemented circular buffer message passing and super-loop architecture |
| Protocol Deadlock | Added timeout mechanism to force protocol progression |
| Byzantine Behavior Modeling | Designed probabilistic fault injection for realistic Byzantine behaviors |
| Resource Limitations | Simplified BFT implementation while preserving essential properties |

## 🔮 Future Work

- **Multi-Device Implementation**: Extend to physical communication between multiple boards
- **Integration with Lightweight Blockchain**: Combine with storage and networking components
- **Performance Optimization**: Further reduce memory and processing requirements
- **Security Enhancements**: Add cryptographic verification of messages
- **Power Analysis**: Evaluate energy efficiency for battery-operated applications

## 👥 Contributors

- Mukund Gupta (Roll No. B22CS086)
- Banoth Sri Kowshika Raj (Roll No. B22CS018)

## 📚 References

1. Castro, M., & Liskov, B. (1999). Practical Byzantine Fault Tolerance. Proceedings of the Third Symposium on Operating Systems Design and Implementation. [Link](https://pmg.csail.mit.edu/papers/osdi99.pdf)

2. Cachin, C., & Vukolić, M. (2017). Blockchain Consensus Protocols in the Wild. [Link](https://github.com/bellaj/Blockchain/blob/master/Blockchain%20Consensus%20Protocols%20in%20the%20Wild.pdf)

3. DIDecentral. PBFT Implementation Reference. [GitHub](https://github.com/didchain/pbft)

4. Samaniego, M., & Deters, R. (2016). Blockchain as a Service for IoT. IEEE International Conference on Internet of Things. [Link](https://ieeexplore.ieee.org/document/7845448)

---

## License

This project is licensed under the MIT License - see the LICENSE file for details.
