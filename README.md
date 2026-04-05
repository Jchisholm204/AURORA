# AURORA
Accelerated User-space Remote Offload for Resilient Applications

> **This Project is a WIP** (Work In Progress)
> The server side implementation is currently missing a few target features.
> The client side library is missing some of the same features.
> For the latest updates on what is broken, check the issues page.

AURORA is an NVIDIA BlueField Accelerated, User-Space checkpoint-restart mechanism that is currently untested.
This codebase was designed to evaluate the usefulness in offloading checkpoint-restart operations to NVIDIA BlueField or other SmartNIC devices.

## Documentation
All relevant documentation can be found under [`./docs`](./docs).

### Building
Steps and instructions for building the code can be found under [`./docs/Building.md`](./docs/Building.md).

### Bindings
The user library binds were chosen to mimic that of [VeloC](https://github.com/ECP-VeloC/VELOC/tree/main).
Therefore, this project is, in theory, "compatible" with any codebase currently using the VeloC Checkpoint Restore mechanism.

## Contributing/Forking
This is a research project I developed for my final Undergraduate Research Project as part of Queen's University ELEC 497.
Anyone is welcome to use, contribute, or otherwise modify this software so long as license terms are followed.
