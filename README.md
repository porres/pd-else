#### Welcome to the readme of EL Locus Solus' Else library for Pd 

# ELSE - EL Locus Solus' Externals

--------------------------------------------------------------------------

### Version 1.0 beta-4 (Unreleased)

The project is still in a beta phase, where drastic changes may occur and backwards compatibility is not guaranteed until a final release. Find latest releases at: https://github.com/porres/pd-else/releases

--------------------------------------------------------------------------

	Copyright © 2017 Alexandre Torres Porres <porres@gmail.com>

   This work is free. You can redistribute it and/or modify it under the
   terms of the Do What The Fuck You Want To Public License, Version 2,
   as published by Sam Hocevar. 
 
   See License.txt and http://www.wtfpl.net/ for more details

--------------------------------------------------------------------------

**"EL Locus Solus"** is run by Alexandre Torres Porres, who organizes cultural events/concerts and teaches computer music courses; website: http://alexandre-torres.wixsite.com/el-locus-solus

EL Locus Solus offers a computer music tutorial with examples in Pure Data for its courses. These are in portuguese as of yet, with plans for english translation and formatting into a book. There are over 350 examples for now and they were designed to work with Pd Extended 0.42-5, making extensive use of the existing objects available in Pd Extended's libraries.

Even though extended has quite a large set of external libraries and objects, at some point there was the need of something "else". Thus, EL Locus Solus is now not only offering computer music examples, but also the "ELSE" library, with extra objects for its didactic material.

ELSE steals some ideas and stuff from SuperCollider, such as most of the chaotic generators UGENs. It also grew to encompass functionalities from objects found in other Pd libraries, but with a different design.  

--------------------------------------------------------------------------

### Downloading ELSE:

Look for the latest releases in https://github.com/porres/pd-else/releases - but ELSE is also available via 'deken' (Pd's external manager). In Pd, just go for Help => Find Externals and search for 'else'.

###Installing ELSE:

This release has been tested with Pd Vanilla 0.47-1, not guaranteed to work in any other version or in Pd Extended/Purr Data. ELSE comes with a set of separate binaries, so you just need to add the "else" path to Pd.

####Building ELSE for Pd Vanilla:

ELSE relies on the build system called "pd-lib-builder" by Katja Vetter (check the project in: <https://github.com/pure-data/pd-lib-builder>). PdLibBuilder tries to find the Pd source directory at several common locations, but when this fails, you have to specify the path yourself using the pdincludepath variable. Example:

<pre>make pdincludepath=~/pd-0.47-1/src/  (for Windows/MinGW add 'pdbinpath=~/pd-0.47-1/bin/)</pre>

* Installing with pdlibbuilder

use "objectsdir" to set a relative path for your build, something like:

<pre>make install objectsdir=../else-build</pre>

Then move it to your preferred install folder for Pd and add it to the path.

--------------------------------------------------------------------------

##Current Object list (150 objects):

**AUDIO PLAYING: [1]**
- [sampler~]

**EXTRA: [2]**
- [args]
- [output~]

**MISCELLANEA: [3]**
- [changed]
- [loadbanger] / [lb]
- [order]

**AUDIO PROCESSING: [3]**
- [downsample~]
- [allpass.rev~]
- [fbdelay~]

**REVERB: [1]**
- [plateverb~]

**PHYSICAL MODELLING: [1]**
- [pluck~]

**OSCILLATORS (DETERMINISTIC GENERATORS): [14]**
- [cosine~]
- [impulse~] / [imp~]
- [impulse2~] / [imp2~]
- [parabolic~]
- [pulse~]
- [sawtooth~]
- [sawtooth2~]
- [oscbank~]
- [oscbank2~]
- [sine~]
- [square~]
- [triangular~]
- [vsaw~]
- [pmosc~]

**CHAOTIC GENERATORS: [18]**
- [brown~] 
- [clipnoise~] 
- [crackle~] 
- [cusp~] 
- [fbsine~] 
- [gbman~] 
- [henon~]
- [ikeda~]
- [latoocarfian~]
- [lfnoise~]
- [lincong~]
- [logistic~]
- [quad~]
- [rampnoise~]
- [randpulse~]
- [standard~]
- [stepnoise~]
- [xmod~]

**FILTERS (16):**
- [allpass.2nd~]
- [bandpass~]
- [bandstop~]
- [bpbank~]
- [bicoeff]
- [biplot]
- [brickwall~]
- [eq~]
- [highpass~]
- [highshelf~]
- [lowpass~]
- [lowshelf~]
- [resonbank~]
- [resonbank2~]
- [resonant~]
- [resonant2~]

**SIGNAL ROUTING: [9]**
- [balance~]
- [pan2~]
- [pan4~]
- [rotate~]
- [xfade~]
- [xgate~]
- [xgate2~]
- [xselect~]
- [xselect2~]

**TRIGGERS: [16]**
- [coin~]
- [dust~]
- [dust2~]
- [pimp~]
- [metro~]
- [pulsecount~]
- [pulsediv~]
- [sh~]
- [tdelay~]
- [tdelay2~]
- [toggleff~]
- [tgate~]
- [match~]
- [trig2bang~]
- [trigger~]
- [trighold~]

**SIGNAL ANALYSIS: [10]**
- [changed~]
- [changed2~]
- [elapsed~]
- [lastvalue~]
- [median~]
- [peak~]
- [rms~]
- [togedge~]
- [vu~]
- [zerocross~]

**CONTROL: [11]**
 - [asr~]
 - [autofade~]
 - [decay~]
 - [decay2~]
 - [fader~]
 - [glide~]
 - [glide2~]
 - [ramp~]
 - [susloop~]
 - [sequencer~]
 - [impseq~]

**GUI: [11]**
- [display]
- [display~]
- [out~]
- [gain~]
- [gain2~]
- [graph~]
- [meter~]
- [meter2~]
- [mix2~]
- [mix4~]
- [setdsp~]

**MATH/LOGIC: [13]**
- [accum~]
- [ceil]
- [ceil~]
- [floor]
- [floor~]
- [fold]
- [fold~]
- [lastvalue]
- [loop]
- [random~]
- [sin~]
- [wrap2]
- [wrap2~]

**CONSTANT VALUES: [4]**
- [nyquist~]
- [pi]
- [sr~]
- [e]

**CONVERSION: [17]**
- [any2symbol]
- [cents2rato]
- [cents2ratio~]
- [db2lin]
- [db2lin~]
- [float2sig~]
- [hz2rad]
- [hz2rad~]
- [lin2db]
- [lin2db~]
- [rad2hz]
- [rad2hz~]
- [ratio2cents]
- [ratio2cents~]
- [rescale]
- [rescale~]
- [sig2float~]