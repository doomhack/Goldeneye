# Setup Guide

<!-- TOC -->
* [Setup Guide](#setup-guide)
  * [Prerequisites](#prerequisites)
  * [Install dependencies](#install-dependencies)
    * [A. Linux / WSL / VM](#a-linux--wsl--vm)
    * [B. Docker](#b-docker)
    * [C. Docker Compose](#c-docker-compose)
  * [Usage](#usage)
    * [Extract assets from base rom(s)](#extract-assets-from-base-roms)
    * [Build the ROM](#build-the-rom)
  * [In-depth build process](#in-depth-build-process)
    * [Requirements](#requirements)
    * [Environment](#environment)
    * [C Compiler](#c-compiler)
    * [Assembly Preprocessor](#assembly-preprocessor)
    * [Code build process](#code-build-process)
    * [Asset/data build process](#assetdata-build-process)
    * [Building the final ROM](#building-the-final-rom)
<!-- TOC -->

## Prerequisites

* clone this repository to a directory of your choosing
* place an unmodified copy of your existing NTSC (US) ROM inside the root of this repository with the name `baserom.u.z64`
  * to build PAL (EU), **additionally** place `baserom.e.z64` into the root dir

## Install dependencies

You can choose one of the following installation methods to get all needed dependencies.

### A. Linux / WSL / VM

**Install build dependencies**

The requirements for Debian / Ubuntu should be:

```bash
sudo apt-get update
sudo apt-get install binutils-mips-linux-gnu gcc-mips-linux-gnu make git python3
```

If you don't have host development tools already installed then you will also need to install `build-essential`:

```bash
sudo apt-get install build-essential
```

### B. Docker

* change directory to the cloned repo
* build the docker image: `docker build -t goldeneye .`
* connect to the container: `docker run --rm -it -v $(pwd):/home/dev goldeneye` (alternatively, replace `$(pwd)` with the absolute path of your directory)

### C. Docker Compose

* run and connect to shell in project root: `docker-compose run ge007 bash`

Alternatively, you can start the container with `docker compose up` and connect to the container in e.g. docker desktop

## Usage

Be careful! If you previously compiled GoldenEye on a different operating system or CPU architecture, the binaries (gzip, n64cksum) that were compiled will be incompatible.
You must delete them.

There may be a "dubious ownership" error from Git, and it may say it fails to detect the Git repository.
Run `git status` to see how to fix it.

### Extract assets from base rom(s)

* to have the files in correct location, see [prerequisites](#prerequisites)

To extract the NTSC (US) base rom assets run the following from root directory:

```bash
./scripts/extract_baserom.u.sh
```

> **Extracting NTSC (US) base rom assets is mandatory before extracting PAL (EU) assets.**

To extract the PAL (EU) base rom assets run the following from root directory:

```bash
./scripts/extract_baserom.u.sh && ./scripts/extract_diff.e.sh
```

Other options to extract base rom assets or extract diff:

```bash
./scripts/extract_baserom.u.sh /path_to/rom.n64 # ROM in another directory
./scripts/extract_baserom.u.sh /mnt/e/Goldeneye.n64 # ROM located on EverDrive
./scripts/extract_baserom.u.sh files # Extract files only
./scripts/extract_baserom.u.sh images # Extract images only
```

**Note: If you are upgrading from an old repository, run:**

```bash
./scripts/clean_baserom.sh && ./scripts/extract_baserom.u.sh && make clean
```

### Build the ROM

Run `make` from root directory to build the ROM (defaults to `VERSION=US`).

```bash
make
```

If all goes well, the ROM is written to `build/u/ge007.u.z64`.

Other examples:

```bash
make -j4                  # build with 4 jobs
make VERSION=EU -j4       # build PAL (EU) version instead
make UNLOCK_ALL=1         # open every mission and difficulty, for testing
```

The full list of configurable variables are listed below, **with the default being the first listed**:

* ``VERSION``: ``US``, ``EU``
* ``UNLOCK_ALL``: ``0``, ``1`` (all missions and difficulties unlocked)

Additional documentation of the build process can be found in the next section.

## In-depth build process

This section explains the details of the build process.

### Requirements

A GCC MIPS cross compiler and binutils (`mips-linux-gnu-`, `mips64-linux-gnu-` or `mips64-elf-` prefix) must be installed.

### Environment

The build uses the `US` version by default. Available options are `US` and `EU`. For example

    make clean VERSION=EU
    make VERSION=EU

### C Compiler

All C code (game, libultra and asset data) is compiled with the GCC MIPS cross compiler at `-O2`.
Header dependencies are tracked automatically, so a plain `make` rebuilds whatever a header change affects.

### Assembly Preprocessor

`GLOBAL_ASM` is retired in this repository, so no asm preprocessor step is used during normal builds.
Source `.c` files are compiled directly by GCC.

### Code build process

`src` and `src/game`: .c and .s files are compiled into .o files

`src/libultra`: .c and .s files are compiled into .o files

### Asset/data build process

Before compilation begins, assets are converted into .c files. This file is then compiled using the c compiler in the usual manner.
Once an .o file exists, it is converted to an .elf file using the toolchain `-ld` program, and a .ld file specification explaining
where the ELF sections should be arranged in the file (and also which sections to exclude). The toolchain `-objcopy` program is
then used to dump the data in the .elf file into a similar .bin file.

The .bin file is then compressed using the standard compression program to produce a .rz file.

For compilation, having a .c file is not necessary as long the correct .bin file is available. This can be created from the extract script.

Each obseg asset category has its own `Makefile` in the obseg folder.

Once the .rz files exist for an asset category, they can be bundled together into an .o file. All obseg assets are bundled
in `assets/obseg/ob_seg.s` and music is bundled in `assets/music/music.s`.

### Building the final ROM

Once all code and assets are compiled into .o files, these are combined into one .elf file. The layout of the object files
is given by the `ge007.*.ld` files in the root of the project. For a list of individual methods, assets, and files see
the map file in `build/[ue]/ge007.*.map` (where `*` is the country code, `u` or `e`).

The toolchain `-objcopy` program is then used to create the bundled .bin of the entire ROM.

The final step is to run the `tools/n64cksum` program on the .bin file to create the final .z64.
