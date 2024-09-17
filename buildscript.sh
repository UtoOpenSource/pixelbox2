#!/bin/bash
mkdir -p build
cd build
cmake .. && make && ./pixelbox
cd ..