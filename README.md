## **AURORA**: Accelerated User-space Remote Offload for Resilient Applications
---

Efficient checkpoint/restart mechanisms are critical for high-performance computing systems, where long-running applications must quickly recover from failures to maintain productivity and resource efficiency. 
However, all current checkpoint/restart mechanisms rely on the host CPU, introducing cache contention and operating-system jitter. 
To address this issue, we present a BlueField-Assisted Resiliency Framework, AURORA: Accelerated User-space Remote Offload for Resilient Applications. 
Unlike traditional methods that consume significant CPU resources, SmartNIC-assisted checkpoint/restart offloads IO-bound processes to the SmartNIC, allowing truly parallel operation between the host and its recovery mechanism. 
Through experimental microbenchmarks, we show that AURORA dramatically reduces host overhead, unlocking up to 10% faster runtimes for CPU-bound HPC workloads when compared to traditional host-based mechanisms.

---

>[!NOTE]
> This Project is a **WIP** (Work In Progress) research tool. 
> Updates may cause breaking changes and documentation may be slightly out of date.
> For the latest updates on what is broken, check the issues page.
## Documentation
All relevant documentation can be found under [`./docs`](./docs). 
Instructions on dependencies and building can be found in [Building](docs/Building.md).
Server usage can be found under the [Server Docs](docs/Server%20Docs/Server%20Docs.md), while user API documentation is under [Client Lib](docs/Client%20Lib/Client%20Lib.md)
Internal documentation, and documentation for the [Common Lib](docs/Common%20Lib/Common%20Lib.md) is also under [Common Lib](docs/Common%20Lib/Common%20Lib.md).
Finally, [Docs/Testing](docs/Testing/Testing.md) contains documentation on how to run the testing scripts and reproduce paper results.
### Building, Installation & Releases
Steps and instructions for building the code can be found under [Building](docs/Building.md).
Installation is handled via CMake packaging, see [Installing](docs/Installing.md) for further details.
Prepackaged releases, along with test logs, are published on the [releases page](https://github.com/Jchisholm204/AURORA/releases). 

### Bindings
The user library binds were chosen to mimic that of [VeloC](https://github.com/ECP-VeloC/VELOC/tree/main).
Therefore, this project is, in theory, "compatible" with any codebase currently using the VeloC Checkpoint Restore mechanism.
For further details on the AURORA User Library, and usage, see the [Client Lib](docs/Client%20Lib/Client%20Lib.md) Docs.

## Contributing/Forking
This code was developed as research project for [Queen's University CAESAR Lab](https://caesar.engineering.queensu.ca/).
All contributions are welcome. See the LICENCE for 