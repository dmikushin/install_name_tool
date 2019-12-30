# A Linux port of Apple's install_name_tool

This port decorates NixOS PatchElf functionality to provide Apple's standard `install_name_tool` utility interface on Linux. This way we can deploy MacOS-specific CMake logic on Linux, such as `fixup_bundle`.

## Buildling

```
git clone
cd install_name_tool
mkdir build
cd build
cmake ..
make
```

## Deployment

Use as a regular MacOS `install_name_tool`, but on Linux:

```
./install_name_tool 
Usage: ./install_name_tool [-change old new] ... [-rpath old new] ... [-add_rpath new] ... [-delete_rpath old] ... [-id name] input
```

## Licenses

The `install_name_pool` is Apple Public Source License, and NixOS PatchElf is GPLv3. Please see the license files for further details.

