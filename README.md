# obs-virtual-background


A virtual background for OBS which uses google's bodyPix to
apply a live mask to a video feed.


## Installation

There are two components to setting this up

- Get obs-studio source code

```
git clone --recursive https://github.com/obsproject/obs-studio.git
```

- Build plugins


```
git clone https://github.com/axiak/obs-virtual-background.git
cd obs-virtual-background
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="../../obs-studio/libobs" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```
