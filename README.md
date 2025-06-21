# XanaduOS

The third iteration of my homebrew Operating System, **XanaduOS**. I decided to make this to push the boundaries of what I could develop - skipping all the intermediates and controlling the hardware directly.

So far, I have implemented several advanced features, such as:
- Multitasking support, with Unix-like *fork*, *exec*, *wait*, et al.
- A work-in-progress LibC via Newlib. This has been used to implement some programs such as `bash`, GNU `bc` (a calculator), and `vi` (limited due to partially implemented TTY calls)!
 
## Screenshots

![image](https://github.com/user-attachments/assets/7dd86343-404d-4683-8300-7277b0156f3d)
Doing some basic looking around with my basic testing shell, Xansh.

![image](https://github.com/user-attachments/assets/ea768bee-d59c-40d8-adbd-231c6d4f70dd)
Framebuffer programs! Displaying a mandelbot fractal overlaid by the iconic shuttle.tga.

![image](https://github.com/user-attachments/assets/4301c0af-13be-450f-9ec1-0ecfd777c681)
Running bash, using it, then GNU BC from bash and doing some calculations, then back to bash, and finally back to Xansh - no problem!



 
### What I'm Doing Now
As of the writing of this README (June 6, 2025), the roadmap is:
- ACPI AML parsing
- PCI Device Enumeration
- Disk access
- Filesystem (EXT2, ISO9660)
- Ramdisk reimplementation (likely squashfs due to familiarity)
- TTY stuff
    - multi-tty handling
    - control characters/mode switch (raw, handled, etc.)
- USB Stack

### What's Next?
- Once the text-based system is stable enough for proper use, a GUI system for programs to read and write with.
  - Likely, this requires at least partial networking capabilities for IPC, since pipes would quickly get unwieldly with multiple GUI processes.
- A userspace-accessible GPU pipeline to allow multiple processes to use the GPU
- Compile flags to optimize specifically for a target device's capabilities
- Once all that is done, whatever logically follows that I've found helpful or necessary while working on this!
