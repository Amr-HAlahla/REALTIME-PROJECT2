# Humanitarian Aid Distribution Simulation

This project simulates a humanitarian aid distribution operation in a hostile environment. It models the process of delivering aid from cargo planes to families in need, while dealing with potential disruptions from hostile forces.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Configuration](#configuration)
- [Components](#components)
- [Installation](#installation)
- [Usage](#usage)

## Overview

The simulation involves multiple processes that represent different entities in the aid distribution chain, including cargo planes, collectors, distributors, and families. The system uses shared memory and semaphores for inter-process communication and synchronization.

## Features

- **Multi-Process Simulation**: Models various entities involved in aid distribution.
- **Shared Memory and Semaphores**: Utilizes POSIX shared memory and semaphores for efficient data sharing and synchronization.
- **Configurable Parameters**: Allows customization of simulation parameters through a configuration file.
- **Hostile Interference Simulation**: Includes occupation forces that attempt to disrupt the operation.
- **Monitoring and Safety**: Tracks container status, worker energy, and family starvation levels.

## Configuration

The simulation is configured using the `configuration.txt` file, which includes parameters such as:

- Number of cargo planes and containers
- Drop frequencies and refill periods
- Number of committees and workers
- Worker energy levels and capacities
- Various thresholds for simulation conditions

## Components

- **parent.c**: Main process that orchestrates the simulation.
- **cargoPlane.c**: Simulates cargo planes dropping aid containers.
- **collectorsCommittee.c**: Manages groups of workers collecting containers.
- **distributers.c**: Handles distribution of aid to families.
- **families.c**: Represents families receiving aid.
- **monitor.c**: Monitors container status and movement.
- **splitter.c**: Splits containers into smaller bags.
- **occupationForces.c**: Simulates hostile forces.

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/Amr-HAlahla/REALTIME-PROJECT2
   cd REALTIME-PROJECT2
   ```

2. Compile the project using the provided Makefile:
   ```bash
   make
   ```

## Usage

Run the simulation with the following command:
```bash
./parent configuration.txt
```

---

## 🗂️ Project Structure

| File/Module             | Description                                                                 |
|-------------------------|-----------------------------------------------------------------------------|
| `parent.c`              | Main orchestrator of the entire simulation.                                |
| `cargoPlane.c`          | Simulates cargo planes dropping aid containers.                           |
| `collectorsCommittee.c`| Committees of workers collecting aid containers after drop.                |
| `splitter.c`            | Processes that split containers into smaller aid bags.                    |
| `distributers.c`        | Responsible for delivering aid bags to families.                          |
| `families.c`            | Simulates families receiving aid and tracking starvation levels.          |
| `monitor.c`             | Continuously tracks the state of containers, worker status, and simulation metrics. |
| `occupationForces.c`    | Hostile forces attempting to disrupt aid operations.                      |
| `configuration.txt`     | Configuration file for tuning simulation parameters.                      |

---

## ⚙️ Features

- **Multi-Stage Aid Distribution Pipeline**  
  1. Cargo planes drop aid containers  
  2. Collectors gather the containers  
  3. Splitters divide containers into aid bags  
  4. Distributers deliver bags to families  

- **Inter-Process Communication (IPC)**  
  - POSIX shared memory for data exchange  
  - Semaphores for process synchronization  
  - Signals for control and monitoring  

- **Simulation of Hostile Interference**  
  - Randomized attacks by occupation forces  
  - Destruction or theft of containers  
  - Worker casualties  

- **Dynamic Resource Management**  
  - Worker energy levels and refueling logic  
  - Monitoring container status (dropped, collected, crashed)  
  - Family starvation level tracking  

- **Termination Criteria**  
  - Excessive destruction of containers  
  - High worker or family death rates  
  - Reaching maximum simulation time  

---

## 🔧 Configuration

The simulation behavior is driven by the `configuration.txt` file. It allows setting:

- Number of:
  - Cargo planes
  - Containers per drop
  - Collector and distributor committees
  - Workers per committee
- Container drop frequency and plane refill periods
- Worker energy levels, consumption rates, and recovery time
- Starvation thresholds and monitoring intervals
- Simulation timeout

Example snippet from `configuration.txt`:

cargo_planes=3 containers_per_drop=10 drop_frequency=5 collectors_committees=2 splitters=4 distributors=2 families=10 monitor_interval=3 max_starvation_level=5 simulation_duration=120

---

## 🚀 How to Build & Run

### Requirements:
- GCC compiler (or any C compiler)
- POSIX-compliant system (Linux/Unix)
- Make utility (optional)

### Build Instructions:

```bash
gcc -o parent parent.c -lrt -lpthread
gcc -o cargoPlane cargoPlane.c -lrt -lpthread
gcc -o collectorsCommittee collectorsCommittee.c -lrt -lpthread
gcc -o splitter splitter.c -lrt -lpthread
gcc -o distributers distributers.c -lrt -lpthread
gcc -o families families.c -lrt -lpthread
gcc -o monitor monitor.c -lrt -lpthread
gcc -o occupationForces occupationForces.c -lrt -lpthread

