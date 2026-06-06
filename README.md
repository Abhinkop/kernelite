# Kernalite

**Kernalite** is a minimalist, educational operating system kernel built from scratch. The project aims to recreate core components of the Linux architecture to explore the fundamentals of operating system design and hardware-software interaction.

## 🎯 Project Goals
* **Multi-Arch Support:** Initially targeting **aarch64** (ARM64), with planned ports for **x86_64** and legacy 32-bit architectures.
* **Linux-Inspired:** Implementing core Unix-like subsystems including memory management, scheduling, and interrupt handling.
* **Educational Clarity:** Maintaining a clean, documented codebase that prioritizes readability over extreme optimization.

## 📂 Project Structure

* **`docs/`**: Technical specifications and Doxygen configuration.
* **`src/boot/`**: Kernel entry point — EL setup and initial CPU handoff from the bootloader.
* **`src/kernel/`**: Core kernel subsystems.
    * **`allocator/`**: Physical page allocator.
    * **`drivers/`**: Hardware drivers (UART).
    * **`error/`**: Panic and error string handling.
    * **`exception_handling/`**: AArch64 exception vector table and fault handler (ESR_EL1 decoding).
    * **`exit/`**: Kernel exit/halt path.
    * **`fdt/`**: Flattened Device Tree parser.
    * **`mmu/`**: MMU setup — MAIR, TCR, SCTLR_EL1, `enable_mmu()`.
    * **`page_table/`**: AArch64 page table descriptor construction and mapping.
    * **`utils/`**: `kprintf`, string utilities.
    * **`main.c`**: Kernel `kmain` entry.
* **`src/include/`**: Public headers mirroring the `src/kernel/` layout, plus the linker script (`linker/linker.lds`).
* **`tests/`**: Host-side unit tests for page table mapping and linker symbol layout.
* **`tools/register_decoder/`**: Standalone utility for decoding AArch64 system register values (SCTLR_EL1).

## 🛠 Building & Running

This project requires a cross-compiler (e.g., `aarch64-none-elf-gcc`) to ensure the kernel is built correctly for the target architecture regardless of your host machine.

### Build System
The project is designed to be built using:
* **Make:** For low-level control over the build process.
* **CMake:** (Optional) For modular project management as the codebase grows.

### Git Submodules
This repository depends on external sources under `external/dtc`, which is tracked as a git submodule.
When cloning this repository, initialize submodules before building:
```bash
git clone --recurse-submodules <repo-url>
cd kernalite
make all
```
If you already cloned without submodules, run:
```bash
git submodule update --init --recursive
```

### Emulation
To test Kernalite on aarch64 using QEMU:
```bash
make all
qemu-system-aarch64 -M virt -cpu cortex-a53 -kernel build/images/Image -nographic
