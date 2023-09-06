# pd.build

The repository offers a set of script to facilitate the creation of [CMake](https://cmake.org/) projects to compile [Pure Data](https://puredata.info/) externals. CMake is a free, open-source and cross-platform system that allows to generate makefiles and projects for many OS and build systems and/or IDEs (Unix makefile, XCode, Visual Studio, Code::Blocks, etc.). So the goal of the pd.build is to offer a system that allows to easily and quickly create projects for developing and compiling Pd externals on the environment of your choice.

***

1. [Pre-required](https://github.com/pierreguillot/pd.build#pre-required)
2. [Configuration](https://github.com/pierreguillot/pd.build#Configuration)
3. [Generation](https://github.com/pierreguillot/pd.build#Generation)
4. [Travis](https://github.com/pierreguillot/pd.build#travis)
5. [Appveyor](https://github.com/pierreguillot/pd.build#appveyor)
6. [Examples](https://github.com/pierreguillot/pd.build#Examples)
7. [See Also](https://github.com/pierreguillot/pd.build#See-Also)

***

## Pre-required

To compile Pd externals using *pd.build*, you need [CMake](https://cmake.org/) (minimum version 2.8) and a build system or an IDE (like Unix makefile, XCode, Visual Studio, Code::Blocks, etc.). You also need the Pure Data sources, that are generally included within your Pure Data distribution and [pd.build](https://github.com/pierreguillot/pd.build/archive/master.zip). If you use [Git](https://git-scm.com/) to manage your project, it is recommend to include pd.build as a submodule `git submodule add https://github.com/pierreguillot/pd.build`. The Pd sources can also be included as a submodule using the [Git repository](https://github.com/pure-data/pure-data).

## Configuration

The configuration of the CMakeList with pd.build is pretty straight forward but depends on how you manage your project (folder, sources, dependencies, etc.). Here is an example that demonstrate the basic usage of the pd.build system:

```cmake
# Define your standard CMake header (for example):
cmake_minimum_required(VERSION 2.8)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_MACOSX_RPATH Off)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.4)
set(CMAKE_OSX_ARCHITECTURES "i386;x86_64")

# Include pd.cmake (1):  
include(${project_source_dir}/pd.build/pd.cmake)

# Declare the name of the project:   
project(my_objects)

# Define the path to the Pure Data sources (2):
set_pd_sources(${project_source_dir}/pure-data/src)

# Set the output path for the externals (3):  
set_pd_external_path(${project_source_dir}/binaries/)

# Compiling for Pd double precision (4)(optional)
set(PD_FLOATSIZE64 "ON")

# Add one or several externals (5):   
add_pd_external(my_object1_project my_object1_name ${project_source_dir}/sources/my_object1.c)

add_pd_external(my_object2_project my_object2_name ${project_source_dir}/sources/my_object2.c)
```

Further information:  
1. The path *pd.cmake* depends on where you installed *pd.build*, here we assume that *pd.build* is localized at the root directory of you project.  
2. The sources of Pure Data are not directly included in the *pd.build* because perhaps someone would like to use a specific version of Pure Data (like Pd-extended). It is possible that a later version directly include the latest Pd sources by default and use this function to override the path if needed.   
3. Here the externals are installed in the *binaries* subfolder but you can use the function to use the folder of your choice.  
4. As of Pd 0.51-0 you can compile a ["Double precision" Pd](http://msp.ucsd.edu/Pd_documentation/x6.htm#s6.6). If you intend to use your externals in such an environment, you must also compile them with double precision by adding this line.
5. The function adds a new subproject to the main project. This subproject matches to a new external allowing to compile only one object without compiling all the others. The first argument is the name of the subproject, the second argument is the name of the external and the third argument are the sources. If you use more than one file, you should use the CMake file system and quotes as demonstrated below:

```cmake
file(GLOB my_object3_sources ${project_source_dir}/sources/my_object3.c ${project_source_dir}/sources/my_object3_other.h ${project_source_dir}/sources/my_object3_other.c)
add_pd_external(my_object3_project my_object3_name "${my_object3_sources}")
```

## Compilation

The generation of the build system or the IDE project is similar to any CMake project. The basic usage follows these steps from the project folder (where *CMakeList* is localized):

```bash
# Create a subfolder (generally called build)
mkdir build
# Move to this folder
cd build
# Generate a project for the default platform
cmake ..
# Or display the available platforms and generate the project for the platform of your choice
cmake --help (optional)
cmake .. -G "Xcode" (example)
cmake .. -G "Unix Makefiles" (example)
cmake .. -G "Visual Studio 14 2015" (example)
# Then use your IDE or use CMake to compile
cmake --build .
```

## Travis

Travis is a continuous integration (CI) server that allows to build, test and deploy your externals online for several operating systems. The pd.build repository also offers a set of scripts that facilitates the set up of the CI with travis. The scripts allows you to compile for Linux 32bit, Linux 64bit and MacOS universal machines. Here is an example on how to use the scripts from the travis yml (generally .travis.yml):

```
# Define your standard travis configuration (for example):
language: c
dist: trusty
sudo: required

notifications:
  email: false
git:
  submodules: true
  depth: 3

# Define the PLATFORM environment variable in the configuration matrix and
# if needed define PACKAGE environment variable (1).
matrix:
  include:
  - os: linux
    compiler: gcc
    env:
      - PLATFORM='linux32'
      - PACKAGE='myproject-linux32'
  - os: linux
    compiler: gcc
    env:
      - PLATFORM='linux64'
      - PACKAGE='myproject-linux64'
  - os: osx
    compiler: gcc
    env:
      - PLATFORM='macos'
      - PACKAGE='myproject-macos'

# Install the pre-required dependencies (2)
install: bash ./pd.build/ci.install.sh

# Generate the project and build the externals (3)
script: bash ./pd.build/ci.script.sh

# Package the files and folders (4)
after_success: bash ./pd.build/ci.package.sh LICENSE README.md src/ binaries/

# Deploy to Github
deploy:
  provider: releases
  skip_cleanup: true
  api_key:
    secure: my_secure_key
  file: $PACKAGE.zip
  on:
    tags: true
```

Further information:  
1. The *PLATFORM* environment variable is used by the ci.install.sh and ci.script.sh scripts. The *PACKAGE* environment variable is optional and is only used by the ci.package.sh script.  
2. The ci.install.sh script installs the 32 bit dependencies for the linux environment.   
3. The ci.script.sh script generates the project using CMake and build the externals for the specified platform.  
4. The ci.package.sh script creates a zip file using the name of the *PACKAGE* environment variable and containing all the files and folders given as arguments.

## Appveyor
Coming soon...

## Examples

* [pd.dummies](https://github.com/pierreguillot/pd.dummies)
* [HoaLibrary-Pd](https://github.com/CICM/HoaLibrary-PD/tree/dev/refactory)
* [paccpp/PdObjects](https://github.com/paccpp/PdObjects)

## See Also

* [pd-lib-builder](https://github.com/pure-data/pd-lib-builder)
* [deken](https://github.com/pure-data/deken)
