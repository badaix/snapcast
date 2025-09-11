# build librist

![librist logo](docs/librist_logo.png)

## libRIST

A library that can be used to easily add the RIST protocol to your application.

## Requirements

#### Fedora

```
sudo dnf install cjson-devel  mbedtls-devel meson ninja-build pkgconf-pkg-config
```

## Compile using meson/ninja (linux, osx and windows-mingw)

1. Install [Meson](https://mesonbuild.com/) (0.47 or higher), [Ninja](https://ninja-build.org/)
2. Alternatively, use "pip3 install meson" and "pip3 install ninja" to install them
3. Run `mkdir build && cd build` to create a build directory and enter it
4. Run `meson ..` to configure meson, add `--default-library=static` if static linking is desired
5. Run `ninja` to compile

## Compile using meson/ninja (windows - Visual Studio 2019)

1. Open a cmd window and type "pip3 install meson" to install meson through Python Package Index
2. Run x64 Native Tools Command Prompt for VS 2019.exe
3. cd to the folder where you downloaded or cloned the librist source code
4. Run the command "meson setup build --backend vs2019"
5. Run the command "meson compile -C build"
6. The compiled library and the tools will be in the build and build/tools folders respectively
7. Alternatively, open librist.sln and build the applications manually if you prefer to use the VS IDE

## Build with Docker

1. Simply do a `docker build` on the Dockerfile in the 'common' subdirectory

## Reconfigure

```
meson setup --reconfigure --default-library=static ..
```
