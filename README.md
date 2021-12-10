# ECE-4900 - Fall 2021 - Final Project:
#### Multi-threaded BLOB Analysis for Video Processing
by Matthew Hait

## Prerequisites

This application has the following dependencies:
- **libavcodec-dev**
- **libavformat-dev**
- **libswscale-dev**
- **libtbb-dev**

## Clone
```bash
git clone **yet to be added...**
```
## Build
```bash
mkdir cmake
cd cmake
cmake ..
make
```

## Description
This repository is a demonstration for Threaded Building Blocks (TBB) for image BLOB recognition in thermal imaging. This application reads images, video, or binary images and returns BLOB centroids with the BLOB's average value.
```bash
./ECE-4900_FINAL_M_Hait [OPTIONS] -i INPUT
````

## Options
```bash
Usage:
  ./ECE-4900_FINAL_M_Hait [OPTION...]

 Required options:
  -i SOURCE  File [*.jpg/*.bin] or folder input

 Optional options:
  -b, --benchmark   Tests performance of arbitrary white 25x25 to 4K 
                    images.
  -c                Compare TBB parallel and sequential
  -h, --help        Print help
  -o, --output arg  Output image map of BLOB centroids
                            1: Output binary output file [*.bof]
                            2: Output image file [*.jpg] (default: 1)
  -s                Run Sequential

 *.BIF Input required options:
  -x arg  Image X size
  -y arg  Image Y size


```