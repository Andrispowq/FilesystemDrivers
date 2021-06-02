# FAT32Driver
This project is a little driver I wrote for my operating system for the FAT32 filesystem

The project itself contains three classes: FAT32Driver, FAT32, and VFS.
The FAT32Driver contains every code for accessing a FAT32 formatted disk. It is implemented with C++ File I/O streams but it can easily be implemneted with a kernel.
The FAT32 class contains some abstraction over the driver, which also acts as some kind of a terminal to test it at runtime, but optimally, it will only provide FAT32_OpenFile pointers and manage them on the driver level.
The VFS class is useless at the moment, but it will be responsible for converting full paths into the different formats that the filesystems accept.