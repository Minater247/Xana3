# XanaduOS

The third iteration of my homebrew Operating System. **XanaduOS** features a variety of advanced features:
- Multitasking support, with Unix-like *fork*, *exec*, *wait*, et al.
- Advanced software capabilities with a work-in-progress LibC. Notable programs which work with XanaduOS:
  - Bash
  - GNU BC (Calculator)
  - Vi (limited due to partially implemented TTY calls)
 
## Screenshots

![image](https://github.com/user-attachments/assets/7dd86343-404d-4683-8300-7277b0156f3d)
Doing some basic looking around with my basic testing shell, Xansh.

![image](https://github.com/user-attachments/assets/ea768bee-d59c-40d8-adbd-231c6d4f70dd)
Framebuffer programs! Displaying a mandelbot fractal overlaid by the iconic shuttle.tga.

![image](https://github.com/user-attachments/assets/4301c0af-13be-450f-9ec1-0ecfd777c681)
Running bash, using it, then GNU BC from bash and doing some calculations, then back to bash, and finally back to Xansh - no problem!



 
### What I'm Doing Now
As of the writing of this README (18 Nov. 2024), I am presently working on (in a private branch):
- A complete rewrite of the VFS to support:
  - Reading from devices other than ramdisks
  - Real filesystem implementations not specific to this OS (SquashFS, Ext2, ISO.9660/ECMA-119)
- A proper ATAPI IDE layer to read from CD

### What's Next?
- Once the text-based system is stable enough for proper use, a GUI system for programs to read and write with.
  - Likely, this requires at least partial networking capabilities for IPC, since pipes would quickly get unwieldly with multiple GUI processes.
- USB/PCI support
- A userspace-accessible GPU pipeline to allow multiple processes to use the GPU
- A proper ACPI parser to determine and use the full capabilities of the hardware
- Compile flags to optimize specifically for a target device's capabilities
- Once all that is done, whatever logically follows that I've found helpful or necessary while working on this!
