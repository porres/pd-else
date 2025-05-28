## Technobear's Mutable Instruments for Pure Data , Mi4Pd

The first set are externals wrapping Mutable Instruments code used for Eurorack modules. 

Its truely appreciated that Emile Gillet / Mutable Instruments has published this code under the MIT license, allowing derivatives, its a very generous act, which Id like to acknowledge.

If use code from this library, you should also make your code open source, and also respect the license for mutable instruments code.

All work in this repo has no guarantee and you use at your own risk.

As derivative works, these externals are **not** supported by Oliver/MI, please read the license in mi/LICENSE for details.
see also http://mutable-instruments.net

Grids comes from :
https://github.com/spoitras/Pd-Grids
developed by Spoitras and Henri David

## Status ##
This externals and patches are still in development, so their interfaces may changes.

## Platforms
- Linux 
- macOS 

note:
mainly I use this on the Organelle and raspberryPI (and derivatives).
I only use for dev/testing on macOS, so only with the version of OS X/ Xcode I have installed.

Windows, may or may not work... I believe it does, but do not regularly use/test it.

## Contributions ##
Its best for users if there is a single source for these externals, so if you wish to extend/fix/contribute, Im very happy to take Pull Requests for enhancements / bug fixes etc.

## Externals ##
brds~
clds_reverb~
clds~
lmnts~
rngs_chorus~
rngs_ensemble~
rngs_reverb~
wrps~
grids

## Folder Structure ##
mi - contains MI code, which has some modifications for this purpose, see git logs for details
Organelle - contains Organelle patches based on this code, included binary externals. basically the source for patches uploaded to patchstorage.

## Building ##

you need to have cmake installed (v2.8.11+), see https://cmake.org for downloading/installation.

```
mkdir build 
cd build
cmake ..
make
```
