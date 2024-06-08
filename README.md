# Reverse Engineered C++ Lumix Driver

> NOTE: this driver is EXTREMELY early in development and expect frequent issues. However, my goal is for this to become a stable, open-sourced driver over time.

This project provides a C++ driver for Panasonic Lumix cameras using the WiFi API used internally by the Lumix Sync app. This is a hobby project and I am not a professional C++ developer, so best code practices may not be used. However, I encourage you to make a Pull Request if you feel inclined.

## Purpose for Building

I have gotten interested into Astrophotography, which as you may expect, requires a camera. I could have bought a new camera, but I already have a good Lumix camera. However, there is no Linux driver out there to work with Astrophotography software. Panasonic offers an official SDK, however, it is Windows only, which will not work with my Raspberry Pi Controller.

I am making the code open source so anyone can build it for any platform (x86 Linux, x86 Windows, Arm on Raspberry Pi, etc.).

This driver is designed to be used with [INDI](https://www.indilib.org/), which is a set of Astrophotography drivers.

## Method of Building

I own a Lumix S5IIX, so this driver is entirely built and tested with this camera in mind. Other cameras should work, but I cannot make any guarantees.

I tried using the USB protocol to control the camera, but I was having issues downloading images from the camera, so I switched to the WiFi API.

To reverse engineer, I have a local WiFi hotspot setup on my computer. I have my phone and camera both connected to that hotspot. Then, I send commands to the camera with the Lumix Sync app and intercept the packets with Wireshark.

## Installation

These are the requirements to install, but this is not an exhaustive list. It you have errors when building, create an "Issue" here on GitHub, as it might be a required library that is not installed.

```bash
# for debian based systems
sudo apt update
sudo apt install git libssl-dev libpoco-dev libpugixml-dev libjpeg-dev libfmt-dev
```

[LibRaw snapshot 202403](https://github.com/LibRaw/LibRaw/commit/12b0e5d60c57bb795382fda8494fc45f683550b8) is required for support of the Lumix S5II cameras. If you need support for these cameras, then you must build from source from this snapshot or later. If you have an older camera (released over 1-2 years ago), installing the latest stable release of LibRaw should be fine.

```bash
# ONLY if your camera was released over 1-2 years ago
sudo apt install libraw
```

Then make a copy of this repository and go into the folder:

```bash
git clone https://github.com/njfdev/liblumix
cd liblumix
```

Make a build folder and go into it:

```bash
mkdir build
cd build
```

Then, build and install the library:

> NOTE: The build can sometimes fail when building the zlib dependency. If this happens, it is usually okay to run it a few more times until it successfully builds.

```bash
cmake ../ -DCMAKE_BUILD_TYPE=Release
make
sudo make install
```
