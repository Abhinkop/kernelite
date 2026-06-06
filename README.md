# Kernalite

**Kernalite** is a minimalist, educational operating system kernel built from scratch. The project aims to recreate core components of the Linux architecture to explore the fundamentals of operating system design and hardware-software interaction.

## 🎯 Project Goals
* **Multi-Arch Support:** Initially targeting **aarch64** (ARM64), with planned ports for **x86_64** and legacy 32-bit architectures.
* **Linux-Inspired:** Implementing core Unix-like subsystems including memory management, scheduling, and interrupt handling.
* **Educational Clarity:** Maintaining a clean, documented codebase that prioritizes readability over extreme optimization.

## 📂 Project Structure

* **docs/**: Technical specifications and Doxygen configuration.
* **src/boot/**: Kernel entry point — EL setup and initial CPU handoff from the bootloader.
* **src/kernel/**: Core kernel subsystems.
    * **allocator/**: Physical page allocator.
    * **drivers/**: Hardware drivers (UART).
    * **error/**: Panic and error string handling.
    * **exception_handling/**: AArch64 exception vector table and fault handler (ESR_EL1 decoding).
    * **exit/**: Kernel exit/halt path.
    * **fdt/**: Flattened Device Tree parser.
    * **mmu/**: MMU setup — MAIR, TCR, SCTLR_EL1, `enable_mmu()`.
    * **page_table/**: AArch64 page table descriptor construction and mapping.
    * **utils/**: `kprintf`, string utilities.
    * **main.c**: Kernel `kmain` entry.
* **src/include/**: Public headers mirroring the `src/kernel/` layout, plus the linker script (`linker/linker.lds`).
* **tests/**: Host-side unit tests for page table mapping and linker symbol layout.
* **tools/register_decoder/**: Standalone utility for decoding AArch64 system register values (SCTLR_EL1).

## 🛠 Building & Running

This project requires a cross-compiler (e.g., `aarch64-none-elf-gcc`) to ensure the kernel is built correctly for the target architecture regardless of your host machine.

### Git Submodules
This repository depends on external sources under `external/dtc`, which is tracked as a git submodule.
When cloning this repository, initialize submodules before building:
```bash
git clone --recurse-submodules https://github.com/Abhinkop/kernalite.git
cd kernalite
make all
```
If you already cloned without submodules, run:
```bash
git submodule update --init --recursive
```

### 🧑‍💻 Development Container
This repository includes a VS Code Dev Container configuration under `.devcontainer/`.

The container is based on `mcr.microsoft.com/devcontainers/base:noble` and installs:
* `clang`, `clang-format`, `clang-tidy`, `clangd`
* `doxygen`
* `lld`, `llvm`, `llvm-dev`, `llvm-runtime`
* `gcc-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`
* `qemu-system-arm`, `qemu-efi-aarch64`, `qemu-utils`
* `gdb-multiarch`
* `make`, `git`, `device-tree-compiler`, `bison`, `flex`, `bc`, `pkg-config`

It also installs the ARM GNU toolchain into `/opt/arm-toolchain` and adds it to the `PATH`, so `aarch64-none-elf-gcc` is available inside the container.

The Dev Container config also enables useful VS Code extensions for this project, including:
* `ms-vscode.cpptools`
* `llvm-vs-code-extensions.vscode-clangd`
* `ms-vscode.makefile-tools`
* `florent-revest.addr2line`
* `xaver.clang-format`
* `plorefice.devicetree`
* `13xforever.language-x86-64-assembly`
* `MKornelsen.vscode-arm64`
* `eamodio.gitlens`

A shared shell history volume is mounted at `/commandhistory` so command history persists across Dev Container sessions.

### Build System
The project is designed to be built using: **Make**

#### Make Targets
Below are the main `make` targets supported by this repository:
* **make all**: Build the kernel image and the `tools/register_decoder` utility.
* **make run**: Build the kernel and launch it under QEMU with semihosting enabled.
* **make docs**: Generate Doxygen documentation into `build/docs/html`.
* **make clean-docs**: Remove generated Doxygen documentation files.
* **make clean**: Clean the build output directory and remove generated binaries.
* **make format**: Run `clang-format` on all C source and header files.
* **make clang-tidy**: Run `clang-tidy` in analysis mode on C source files.
* **make clang-tidy-fix**: Run `clang-tidy` with automatic fixes enabled.
* **make submodules**: Initialize or update git submodules recursively.
* **make tools/register_decoder**: Build the register decoder utility located in `tools/register_decoder/`.

### Emulation
To run Kernalite on aarch64 using QEMU:
```bash
make all
qemu-system-aarch64 -machine virt \
-cpu cortex-a57 -nographic -kernel build/images/Image -no-reboot \
-semihosting
```
