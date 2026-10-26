# Goldeneye 007

## About this repository

This is a fork of the Goldeneye 007 decompilation that adds new features (Expansion Pak, hi-res,
split-screen co-op and counter-op). It is built with a modern GCC MIPS cross compiler and no longer
aims to match the original ROMs byte for byte.

It builds the NTSC (US) and PAL (EU) versions. Assets are extracted from these original ROMs:

* baserom.u.z64 `sha1: abe01e4aeb033b6c0836819f549c791b26cfde83`
* baserom.e.z64 `sha1: 167c3c433dec1f1eb921736f7d53fac8cb45ee31`

**Note: This repository does not include all assets necessary for compiling the ROMs. A prior copy of the game is required to extract the assets.**

## Documentation

* [Setup Guide:](./docs/SetupGuide.md) useful information about installing the necessary dependencies and how to use it
* [Structure Guide:](./docs/StructureGuide.md) learn more about the project structure of this repository
* [Style Guide:](./docs/StyleGuide.md) code style conventions if you want to contribute code
