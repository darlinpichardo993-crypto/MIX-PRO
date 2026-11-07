#!/bin/sh
# JUCE-free DSP/analysis/preset tests (any platform with a C++17 compiler)
set -e
c++ -std=c++17 -O2 -Wall -ISource Tests/test_core.cpp -o build-core-tests && ./build-core-tests
