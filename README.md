[![Build Status](https://travis-ci.com/DTolm/VulkanComputeSamples-Transposition.svg?branch=main)](https://travis-ci.com/DTolm/VulkanComputeSamples-Transposition)
# Vulkan Compute Sample - out-of-place transposition 
This repository contains code samples used at EUROfusion Webinar 5. Recording of the webinar can be found at https://www.youtube.com/watch?v=YD_QO5Ilz2U

## Project goals
  - Give an example code of a Vulkan Compute application with comments
  - Show importance of memory coalescing and shared memory bank conflicts. More information on this topic can be found here: https://developer.nvidia.com/blog/efficient-matrix-transpose-cuda-cc/

## Installation
Sample CMakeLists.txt file configures project based on VulkanTransposition.c file with shaders located in shaders/ folder.

## Command-line interface
Vulkan Compute transposition sample has a command-line interface with the following set of commands:\
-h: print help\
-devices: print the list of available GPU devices\
-d X: select GPU device (default 0)\
-c X: specify how much memory is coalesced per transfer: 4-128 bytes (default Nvidia: 32, Intel/AMD: 64, default: 64)\
-size X: specify square array size: should be >= coalescedMemory (default 2048)

## Contact information
Initial version of Vulkan Compute transposition sample is developed by Tolmachev Dmitrii\
Peter Grünberg Institute and Institute for Advanced Simulation, Forschungszentrum Jülich,  D-52425 Jülich, Germany\
E-mail 1: <d.tolmachev@fz-juelich.de>\
E-mail 2: <dtolm96@gmail.com>
