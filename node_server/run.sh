#!/bin/bash
cd $(dirname "$0")
CUDA_LIBRARY_PATH="${CUDA_HOME:-/usr/local/cuda-10.0}/lib64"
TF_FORCE_GPU_ALLOW_GROWTH=true LD_LIBRARY_PATH=$CUDA_LIBRARY_PATH:$LD_LIBRARY_PATH exec node server.js

