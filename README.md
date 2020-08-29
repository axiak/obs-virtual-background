# obs-virtual-background


A virtual background filter for OBS which uses google's bodyPix to
apply a live mask to any video source.


![](https://github.com/axiak/obs-virtual-background/raw/master/img/screencast.gif)


## Background

I initially started with the work Benjamin Elder wrote up at https://elder.dev/posts/open-source-virtual-background/.
Disliking the python/node blend I wrote up a version of this which was node-only, using opencv and custom V4L2 FFI code.

This version had some poor latency behavior so I eventually found myself wanting a filter built into OBS. Unfortunately
the bodyPix model is only available in javascript, so there are two distinct components.

## Theory of operation

There are two components: the node server (in the `node_server` directory), and the OBS plugin (in root/build).

The node server writes a temp file with a TCP port that it will listen on (on localhost), and the OBS filter will, when
it detects the file, attempt to connect to the server and run the filter. Note that the filter will somewhat gracefully
handle a case where the server breaks -- the mask just won't update. 

## Installation


### Requirements

The following needs to be installed on your system:

- FFmpeg
- CUDA 10 (I have both 11 and 10, with 10 in /usr/local/cuda-10.0)
- OpenCV 4
- OBS (duh)

In addition, you'll need the OBS source available locally to build the plugin.



### Setting up the node server

First you'll have to install all the NPM depenencies. The node opencv library needs
an environment variable set, so there's a handy `install.sh` file to help you.

```bash
git clone https://github.com/axiak/obs-virtual-background.git
cd obs-virtual-background/node_server
./install.sh
```

To run, just use the `run.sh` script:

```bash
CUDA_HOME=/usr/local/my-cuda-path ./run.sh
```


### Installing the OBS plugin:


You'll need to download the OBS-studio source and rever to it when running
the CMake build. Here's a quick example that assumes you're starting in some parent directory:

```bash
git clone --recursive https://github.com/obsproject/obs-studio.git
git clone https://github.com/axiak/obs-virtual-background.git

cd obs-virtual-background
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="../../obs-studio/libobs" -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## Todo

- The node server works fairly well but is in need of a refactor. I plan on extracting the protocol logic from the segmentation logic.
- I'm on the lookout for a suitable segmentation model in C/C++ to remove the need for the node service entirely.