EL Locus Solus' Else library for Pd 

ELSE - EL Locus Solus' Externals

The project is still at an alpha experimental phase, where drastic changes may occur and backwards compatibility is not guaranteed for future releases. Find latest releases at: https://github.com/porres/pd-else/releases

----------------------------------------------


"EL Locus Solus" is run by Alexandre Torres Porres, who organizes cultural events and teaches computer music courses since around 2009, at the time of PdCon09 (the 3rd International Pure Data Convention in 2009, São Paulo - Brasil); website: http://alexandre-torres.wixsite.com/el-locus-solus

EL Locus Solus offers a computer music tutorial with examples in Pure Data for its courses. These are in portuguese as of yet, with plans for english translation and formatting into a book. There are over 350 examples for now and they were designed to work with Pd Extended 0.42-5, making extensive use of the existing objects available in Pd Extended's libraries.

Even though extended has quite a large set of external libraries and objects, at some point there was the need of something "else". Thus, EL Locus Solus is now not only offering computer music examples, but also the "else" library, with extra objects for its didactic material.

-------

<strong>Installing ELSE:</strong>

This release has been tested with Pd Vanilla 0.47-1, not guaranteed to work in any other version or in Pd Extended/Purr Data. ELSE comes with a set of separate binaries, so you just need to add the "else" path to Pd.

<strong>Building ELSE for Pd Vanilla:</strong>

ELSE relies on the build system called "pd-lib-builder" by Katja Vetter (check the project in: <https://github.com/pure-data/pd-lib-builder>). PdLibBuilder tries to find the Pd source directory at several common locations, but when this fails, you have to specify the path yourself using the pdincludepath variable. Example:

<pre>make pdincludepath=~/pd-0.47-1/src/  (for Windows/MinGW add 'pdbinpath=~/pd-0.47-1/bin/)</pre>

* Installing with pdlibbuilder

use "objectsdir" to set a relative path for your build, something like:

<pre>make install objectsdir=../else-build</pre>

Then move it to your preferred install folder for Pd and add it to the path.

-------

Current Object list (134 objects):

AUDIO PROCESSING: [4]
- [downsample~]
- [apass2~]
- [fbcomb~]
- [fbdelay~]

PHYSICAL MODELLING: [1]
- [pluck~]

OSCILLATORS (DETERMINISTIC GENERATORS): [13]
- [cosine~]
- [impulse~] / [imp~]
- [impulse2~] / [imp2~]
- [parabolic~]
- [pulse~]
- [sawtooth~]
- [sawtooth2~]
- [oscbank~]
- [sine~]
- [square~]
- [triangular~]
- [vsaw~]
- [pmosc~]

CHAOTIC GENERATORS: [17]
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
- [standard~]
- [stepnoise~]
- [xmod~]

FILTERS (15):
- [apass~]
- [bandpass~]
- [bandstop~]
- [bpbank~]
- [bicoeff]
- [biplot]
- [eq~]
- [highpass~]
- [highshelf~]
- [lowpass~]
- [lowshelf~]
- [resonbank~]
- [resonbank2~]
- [resonant~]
- [resonant2~]

SIGNAL ROUTING: [9]
- [balance~]
- [pan2~]
- [pan4~]
- [rotate~]
- [xfade~]
- [xgate~]
- [xgate2~]
- [xselect~]
- [xselect2~]

TRIGGERS: [12]
- [coin~]
- [dust~]
- [dust2~]
- [pimp~]
- [pulsecount~]
- [pulsediv~]
- [sh~]
- [tdelay~]
- [tdelay2~]
- [toggleff~]
- [tgate~]
- [trigate~]

SIGNAL ANALYSIS: [10]
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

CONTROL: [8]
 - [autofade~]
 - [decay~]
 - [decay2~]
 - [fader~]
 - [glide~]
 - [glide2~]
 - [ramp~]
 - [susloop~]

GUI: [10]
- [display]
- [out~]
- [gain~]
- [gain2~]
- [graph~]
- [meter~]
- [meter2~]
- [mix2~]
- [mix4~]
- [setdsp~]

MATH/LOGIC: [13]
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

CONSTANT VALUES: [4]
- [nyquist]
- [pi]
- [sr~]
- [e]

CONVERSION: [17]
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

MANAGE LISTS: [1]
- [order]
