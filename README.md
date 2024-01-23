# BadApplePkg
Bad Apple!! but it's a UEFI application

[Video here](https://photos.app.goo.gl/FgNDL5g7QgZhf31SA)

## Setup

### Package dependencies
```
# Install ffmpeg, e.g.,
sudo apt install ffmpeg
# Install Python dependencies.
python3 -m pip install ffmpeg-python, pillow
```

### Repo setup
```
# Set up EDK2.
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init --recursive
# Set up this repo.
git clone http://github.com/centipeda/BadApplePkg.gits
```

## Building

### Creating video file

```
python3 convert.py
```

### Building EFI Application

```
cd edk2
source edksetup.sh
# Change build parameters as needed.
build -a X64 -t GCC5 -b RELEASE -p BadApplePkg/BadApplePkg.dsc
```

### Deploying

Use your favorite method for deploying a UEFI application. For example, to launch from a flash drive,
create a FAT32 partition on a USB stick and copy the files to it:

```
cp Build/BadApple/RELEASE_GCC5/X64/BadApple.efi /path/to/drive/BOOT/EFI/BOOTX64.efi
cp BadApple.bin /path/to/drive/BadApple.bin
```
