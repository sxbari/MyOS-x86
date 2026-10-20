## MyOS — Custom x86_64 Operating System

MyOS is a custom 64-bit operating system developed from scratch using **C and x86_64 Assembly**, booted through the Limine bootloader and tested using QEMU.

The project aims to explore and practically implement fundamental **Operating System concepts**, combining low-level kernel development with an interactive retro-style graphical desktop.

### Features Implemented
- **Kernel Development:** Custom x86_64 kernel with interrupt handling and hardware interaction.
- **CPU Scheduling:** Preemptive Round Robin scheduling with independently executing kernel tasks.
- **Memory Management:** Basic dynamic heap allocation and physical memory information using Limine's memory map.
- **Interrupts & Timer:** IDT, PIC, keyboard and mouse interrupts, and PIT-based system timing.
- **File Management:** RAM-based filesystem supporting file creation, reading, writing, listing, and deletion.
- **Graphical Interface:** Custom purple/pink desktop with interactive console, file icons, native file viewer, and taskbar.
- **Mouse & Keyboard Support:** PS/2 input drivers with clickable GUI controls.
- **Desktop Companion:** Animated pixel-art character with blinking, movement, and command-triggered reactions.

### Future Development
Physical memory allocation, virtual memory and paging, process management, system calls, synchronization, and advanced filesystem functionality.

**Tech Stack:** C, x86_64 Assembly, Limine, QEMU, GDB, GNU Make, Git, Linux/WSL.
