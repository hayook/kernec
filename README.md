# Kernec: The Programmable OS Simulator

Kernec is a minimalist operating system simulator that lets you write high-level kernel policies in Lua while the C core handles the simulation main loop and timing.

---

## How it Works
- **C runtime:** Simulates software and hardware interrupts.
- **Lua kernel:** Your playground. Implement scheduling, resource management, and event handling in real-time.

## Quick Start

### Prerequisites
- gcc
- make
- Lua 5.3 shared library & development headers

### Build
```bash
make kernec
```

### Run
```bash
make run
```
