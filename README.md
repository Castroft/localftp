# LFTP - Local File Transfer Tool

## Project Overview

LFTP (Local FTP) is a C-based local network file transfer tool that support automatic device doscovery in local area networks and file upload/download operations.
The project adopts a client-server architecture with features including automatic device discovery, secure file transfer, and a user-friendly command-line interface.

## Project Structure


```
LFTP/
├── build/                    # Build directory
│   ├── Makefile
│   ├── lftp                 # Compiled executable file
│   └── build.sh             # Build script
├── cli/                     # Command-line interface
│   ├── shell.c              # Shell implementation
│   ├── main.c               # Main program entry
│   └── Makefile
├── common/                  # Common modules
│   ├── client.c             # Client implementation
│   ├── server.c             # Server implementation
│   ├── network.c            # Network communication
│   ├── cmd_parser.c         # Command parser
│   ├── discovery_threads.c  # Discovery system threads
│   ├── device_manager.c     # Device management
│   ├── utils.c              # Utility functions
│   └── Makefile
└─── include/                 # Header files directory
    ├── color.h              # Color definitions
    ├── discovery.h          # Device discovery
    ├── shell.h              # Shell-related
    └── transfer.h           # File transfer

```

## Quick Start

1. compile the Project

```bash
# Go to project build directory
cd LFTP/build

# Run build script
./build.sh
```

2. start the Program


```bash
./build/lftp
```


3. Basic Usage

After start the program, use `help` to see all command lines


## Notice

1. port
  UDP port : 5050
  TCP port : 5060
  
  Make sure these two ports are available when the program start

2. OS
   Linux / Unix

3. Firewall
   Ensure the above ports are not blocked


## Future implements

+ multiple files transfer
+ TSL transfer encryption
+ transfer progress bar display
+ Resume intterrupted transfers

performance:
+ use epoll instead of select
+ Mutli-threads downloads
+ tranfer queue management

## Contact

For questions or suggestions,please contact
Email: 3585045923@qq.com

NOTE: this is an education project suitable for learning network programming, multithreaded programming, and linux system programming.
when using in production environments, please consider adding more security and error handing mechanisms.

