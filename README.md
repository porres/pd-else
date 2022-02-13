--------------------------------------------------------------------------

# ELSE - EL Locus Solus' Externals   

### for the Pure Data programming language

### Version: 1.0-0 rc-1 (release candidate #1) With Live Electronics Tutorial 

###   Unreleased 



	Copyright © 2017-2022 Alexandre Torres Porres




   This work is free. You can redistribute it and/or modify it under the
   terms of the Do What The Fuck You Want To Public License, Version 2,
   as published by Sam Hocevar. See License.txt <https://github.com/porres/pd-else/blob/master/License.txt> and <http://www.wtfpl.net/> for more details

   Other licenses may apply for specific objects and this is informed in the source code (example: the [giga.rev~] object).



--------------------------------------------------------------------------

###   About ELSE



This version of ELSE needs **Pd 0.52-1* or above.

ELSE is a big library of externals that extends the performance Pure Data (Pd) - Miller S. Puckette's realtime computer music environment (download Pd from: http://msp.ucsd.edu/software.html).

ELSE provides a cohesive system for computer music, it also serves as a basis for an Live Electronics Tutorial by the same author, yours truly, Alexandre Torres Porres. This tutorial is also found as part of the download of the ELSE library. This library's repository resides at <https://github.com/porres/pd-else/>.

​	Note that you can also download Camomile with support for ELSE externals, see https://github.com/emviveros/Camomile-ELSE/releases/

​	This project is still in a beta phase, where  changes may occur and backwards compatibility is not guaranteed until a final release is available. 



--------------------------------------------------------------------------

### Downloading ELSE:

​	You can get ELSE from https://github.com/porres/pd-else/releases - where all releases are available, but ELSE is also found via Pd's external manager (In Pd, just go for Help => Find Externals and search for 'else').  In any case, you should download the folder to a place Pd automatically searches for, and the common place is the ~/documents/pd/externals folder.

​	Note that since version 1.0-0 beta 28,  the downloads of ELSE also contain a "live eletronics tutorial" as part of the package (as mentioned above). Look for the 'live-electronics-tutorial' folder inside it and also check its README on how to install it.

​	Instructions on how to build ELSE are provided below.



--------------------------------------------------------------------------

### Installing ELSE:

​	ELSE comes as a set of separate binaries and abstractions, so it works if you just add its folder to the path or use [declare -path else]. ELSE comes with a binary that you can use load via "Preferences => Startup" or with [declare -lib else], but all that this does is print information of what version of ELSE you have when you open Pd.  You can also just load the 'else' external for that same purpose, check its help file. 

​	It's important to stress this release needs Pd Vanilla 0.52-0 or above (Pd Extended/Purr Data aren't supported). 



--------------------------------------------------------------------------

### More About ELSE

**"EL Locus Solus"** is run by me, Alexandre Torres Porres, and it organizes cultural events/concerts and music technology courses (<http://alexandre-torres.wixsite.com/el-locus-solus> ) where a Live Electronics tutorial is provided with examples in Pure Data for its courses. These have been recently translated and completely rewritten to english with plans of being accompanied by a book. The latest releases are available at: <https://github.com/porres/Live-Electronic-Music-Tutorial>. This tutorial solely depends on the ELSE library and is a great didactic companion to this library. Both the library and the tutorial are provided as a single download, directly via Pure Data or GitHub.

The examples from the first incarnation of this tutorial were first developed for the now abandoned Pd Extended, making extensive use of the existing objects available in Pd Extended's libraries. Even though Pd Extended had many externals, there was the need at some point for something "else" - thus, this library emerged with the goal of providing more objects to include missing functionalities in the Pd Ecossystem.

But the library grew to encompass functionalities found in other Pd objects/libraries from old Pd Extended as well, with a different design and more functionalities. This was done in order to remove ALL the dependencies of the didactic material from these other libraries - with the goal to rely on just a single library that's alive (in active development) instead of many projects that are now long gone abandoned or not receiving much attention. I'm also involved in maintaining Cyclone, a legacy library for Pd (see: <https://github.com/porres/pd-cyclone>). But ELSE also superseeds cyclone for the purposes of this didactic material.

The goal of ELSE also outgrew the didactic material and includes now objects not necessarily depicted in the computer music examples. Moreover, even basic elements from Pd Vanilla are being redesigned into new objects. So that's it, ELSE is becoming a quite big library and keeps growing and growing. 

It will still take a little while for ELSE to stabilize into a final version. For now, it's at an early "Beta" stage of development, where drastic changes may occur and backwards compatibility is not guaranteed until a final release is available. 



--------------------------------------------------------------------------

#### Building ELSE for Pd Vanilla:

ELSE relies on the build system called "pd-lib-builder" by Katja Vetter (check the project in: <https://github.com/pure-data/pd-lib-builder>). PdLibBuilder tries to find the Pd source directory at several common locations, but when this fails, you have to specify the path yourself using the pdincludepath variable. Example:

<pre>make pdincludepath=~/pd-0.52-0/src/  (for Windows/MinGW add 'pdbinpath=~/pd-0.52-0 /bin/)</pre>

* Installing with pdlibbuilder

Go to the pd-else folder and use "objectsdir" to set a relative path for your build, something like:

<pre>make install objectsdir=../else-build</pre>
Then move it to your preferred install folder for Pd and add it to the path.

Cross compiling is also possible with something like this

<pre>make CC=arm-linux-gnueabihf-gcc target.arch=arm7l install objectsdir=../</pre>



--------------------------------------------------------------------------

### Acknowledgements

​	Special thanks to Flávio Luis Schiavoni, for helping me out in a few things when I first started coding and collaborating with the objects: [median~] and [keyboard]. 

​	I'd also like to thank my cyclone buddies Derek Kwan and Matt Barber, cause I started learning how to code externals with them as part of the cyclone team. Other developers of cyclone need to be praised, like Czaja, the original author, as I did steal quite a bit from cyclone into ELSE. I'd like to give a special thanks for Matt Barber for developing the "magic" in cyclone that I'm using here and also collaborating to ELSE with the objects: [float2bits], [brown~],  [gray~], [perlin~] and [pinknoise~] .

​	For last, I need to thank my buddy Esteban Viveros for helping with the compilation of ELSE for Camomile.



--------------------------------------------------------------------------

## Current Object list (444 objects):

**ASSORTED: [01]**

- [else]

**FFT: [02]**

- [hann~]
- [bin.shift~]

**TUNING/NOTES: [16]**

- [scales]
- [autotune]
- [autotune2]
- [retune]
- [eqdiv]
- [cents2scale]
- [scale2cents]
- [frac2cents]
- [cents2frac]
- [frac2dec]
- [dec2frac]
- [midi2freq]
- [freq2midi]
- [pitch2note]
- [note2pitch]
- [note2dur]

**PATCH/SUBPATCH MANAGEMENT: [20]**

- [args]
- [meter]
- [presets]
- [dollsym]
- [receiver]
- [retrieve]
- [blocksize~]
- [click]
- [properties]
- [fontsize]
- [canvas.active]
- [canvas.bounds]
- [canvas.gop]
- [canvas.pos]
- [canvas.edit]
- [canvas.vis]
- [canvas.setname]
- [canvas.name]
- [canvas.zoom]
- [loadbanger] / [lb]

**GENETAL MESSAGE MANAGEMENT: [27]**

- [format]
- [nmess]
- [unite]
- [separate]
- [symbol2any]
- [any2symbol]
- [buffer]
- [changed]
- [hot]
- [initmess]
- [message]
- [default]
- [pack2]
- [pick]
- [limit]
- [spread]
- [router]
- [routeall]
- [routetype]
- [selector]
- [stack] 
- [store] 
- [morph]
- [interpolate]  
- [sig2float~] / [s2f~]
- [float2sig~] / [f2s~]
- [pipe2]

**LIST/MESSAGE MANAGEMENT: [17]**

- [break] 
- [order]
- [combine]
- [group] 
- [iterate]
- [insert]
- [scramble]
- [sort]
- [reverse]
- [rotate]
- [sum]
- [stream]
- [slice] 
- [merge]
- [unmerge]
- [amean]
- [gmean]

**FILE MANAGEMENT: [01]**

- [dir]

**MIDI: [20]**

- [midi]
- [midi.learn]
- [sysrt.in]
- [sysrt.out]
- [ctl.in]
- [ctl.out]
- [touch.in]
- [touch.out]
- [pgm.in]
- [pgm.out]
- [bend.in]
- [bend.out]
- [note.in]
- [note.out]
- [midi.clock]
- [noteinfo]
- [panic]
- [mono]
- [voices]
- [suspedal]

**MATH: FUNCTIONS: [32]**

- [add~]
- [add]
- [median]
- [avg]
- [mov.avg]
- [count]
- [gcd]
- [lcm]
- [frac+]
- [frac*]
- [ceil]
- [ceil~]
- [factor]
- [floor]
- [floor~]
- [trunc]
- [trunc~]
- [rint~]
- [rint]
- [quantizer~]
- [quantizer]
- [fold]
- [fold~]
- [lastvalue]
- [mag]
- [mag~]
- [sin~]
- [wrap2]
- [wrap2~]
- [op~]
- [op]
- [cmul~]

**MATH: CONVERSION: [28]**

- [hex2dec]
- [bpm]
- [dec2hex]
- [car2pol]
- [car2pol~]
- [cents2ratio]
- [cents2ratio~]
- [ms2samps]
- [ms2samps~]
- [db2lin]
- [db2lin~]
- [float2bits]
- [hz2rad]
- [lin2db]
- [lin2db~]
- [deg2rad]
- [rad2deg]
- [pz2coeff]
- [coeff2pz]
- [rad2hz]
- [ratio2cents]
- [ratio2cents~]
- [samps2ms]
- [samps2ms~]
- [pol2car]
- [pol2car~]
- [rescale]
- [rescale~]

**MATH: CONSTANT VALUES: [04]**

- [sr~]
- [nyquist~]
- [pi]
- [e]


**LOGIC: [01]**

- [loop]

**AUDIO PROCESSING: ASSORTED [24]**

- [downsample~]
- [conv~]
- [chorus~]
- [del~]
- [fbdelay~]
- [ffdelay~]
- [rdelay~]
- [shaper~]
- [crusher~]
- [drive~]
- [power~]
- [flanger~]
- [freq.shift~]
- [pitch.shift~]
- [stretch.shift~]
- [ping.pong~]
- [rm~]
- [tremolo~]
- [vibrato~]
- [vocoder~]
- [morph~]
- [freeze~]
- [pvoc.freeze~]
- [phaser~]

**AUDIO PROCESSING: DYNAMICS [05]**

- [compress~]
- [duck~]
- [expand~]
- [noisegate~]
- [norm~]

**AUDIO PROCESSING: REVERBERATION: [09]**

- [allpass.rev~]
- [comb.rev~]
- [echo.rev~]
- [mono.rev~]
- [stereo.rev~]
- [free.rev~]
- [giga.rev~]
- [plate.rev~]
- [fdn.rev~]

**AUDIO PROCESSING: FILTERS [23]:**

- [allpass.2nd~]
- [allpass.filt~]
- [comb.filt~]
- [lop.bw~]
- [hip.bw~]
- [biquads~]
- [bandpass~]
- [bandstop~]
- [crossover~]
- [bpbank~]
- [bicoeff]
- [brickwall~]
- [eq~]
- [highpass~]
- [highshelf~]
- [lowpass~]
- [lowshelf~]
- [mov.avg~]
- [resonbank~]
- [resonbank2~]
- [resonant~]
- [resonant2~]
- [svfilter~]

**BUFFER/SAMPLING/PLAYING/GRANULATION: [14]**

- [table~]
- [player~]
- [gran.player~]
- [pvoc.player~]
- [pvoc.live~]
- [grain.sampler~]
- [grain.live~]
- [batch.rec~]
- [batch.write~]
- [rec.file~]
- [play.file~]
- [tabplayer~]
- [tabwriter~]
- [sample~]

**SYNTHESIS: GRANULAR SYNTHESIS: [01]**

- [grain.synth~]

**SYNTHESIS: PHYSICAL MODELLING: [01]**

- [pluck~]

**SYNTHESIS: OSCILLATORS (DETERMINISTIC GENERATORS): [25]**

- [cosine~]
- [impulse~] / [imp~]
- [impulse2~] / [imp2~]
- [parabolic~]
- [pulse~]
- [saw~]
- [saw2~]
- [oscbank~]
- [oscbank2~]
- [sine~]
- [square~]
- [tri~]
- [gaussian~]
- [vsaw~]
- [pmosc~]
- [wavetable~] / [wt~]
- [bl.imp~]
- [bl.imp2~]
- [bl.saw~]
- [bl.saw2~]
- [bl.sine~]
- [bl.square~]
- [bl.tri~]
- [bl.vsaw~]
- [bl.wavetable~]

**SYNTHESIS: CHAOTIC/NOISE GENERATORS: [25]**

- [brown~] 
- [clipnoise~] 
- [perlin~] 
- [crackle~] 
- [cusp~] 
- [fbsine~] 
- [fbsine2~] 
- [gbman~] 
- [gray~] 
- [henon~]
- [ikeda~]
- [latoocarfian~]
- [lorenz~]
- [lfnoise~]
- [lincong~]
- [logistic~]
- [quad~]
- [rampnoise~]
- [randpulse~]
- [randpulse2~]
- [standard~]
- [stepnoise~]
- [pinknoise~]
- [xmod~]
- [xmod2~]

**CONTROL: MOUSE INTERACTION [2]:

- [mouse]
- [canvas.mouse]

**CONTROL: FADER/PANNING/ROUTING: [15]**

- [fader~]
- [autofade~]
- [autofade2~]
- [balance~]
- [pan2~]
- [pan4~]
- [pan8~]
- [spread~]
- [rotate~]
- [xfade~]
- [xgate~]
- [xgate2~]
- [xselect~]
- [xselect2~]
- [mtx~]

**CONTROL: SEQUENCERS: [9]**

- [euclid]
- [score]
- [score2]
- [pattern]
- [sequencer]
- [sequencer~]
- [impseq~]
- [rec]
- [rec2]

**CONTROL: ENVELOPES [6]**

- [adsr~]
- [asr~]
- [decay~]
- [decay2~]
- [envelope~]
- [envgen~]

**CONTROL: RAMP, LINE GENERATORS / LINE SMOOTHENING [13]**

- [ramp~] 
- [susloop~]
- [function~]
- [slew]
- [slew2]
- [slew~]
- [slew2~]
- [lag~]
- [lag2~]
- [glide]
- [glide2]
- [glide~]
- [glide2~]

**CONTROL: RANDOM: [14]**

- [rand.f]
- [rand.f~]
- [rand.i]
- [rand.i~]
- [rand.list]
- [rand.seq]
- [markov]
- [drunkard~]
- [drunkard]
- [randpulse]
- [randpulse2]
- [lfnoise]
- [stepnoise]
- [rampnoise]
 
 **CONTROL: CONTROL RATE LFOs [5]**

 - [lfo]
 - [phasor]
 - [pimp]
 - [impulse]
 - [pulse]

**CONTROL: TRIGGERS: [27]**

- [above]
- [above~]
- [bangdiv]
- [chance]
- [chance~]
- [dust~]
- [dust2~]
- [gatehold~]
- [gate2imp~]
- [pimp~]
- [pimpmul~]
- [pulsecount~]
- [pulsediv~]
- [sh~]
- [schmitt]
- [schmitt~]
- [status]
- [status~]
- [trig.delay~]
- [trig.delay2~]
- [toggleff~]
- [timed.gate]
- [timed.gate~]
- [match~]
- [trig2bang]
- [trig2bang~]
- [trighold~]

**CONTROL: TRIGGERS: CLOCK [7]**

- [clock]
- [metronome]
- [metronome~]
- [polymetro]
- [speed]
- [tempo]
- [tempo~]

**ANALYSIS: [14]**

- [changed~]
- [changed2~]
- [detect~]
- [lastvalue~]
- [median~]
- [peak~]
- [tap]
- [range]
- [range~]
- [maxpeak~]
- [rms~]
- [mov.rms~]
- [vu~]
- [zerocross~]

**GUI: [36]**

- [drum.seq]
- [gui]
- [pad]
- [messbox]
- [mtx.ctl]
- [biplot]
- [zbiplot]
- [pic]
- [colors]
- [function]
- [circle]
- [slider2d]
- [display]
- [display~]
- [out1~]
- [out~]
- [out4~]
- [out8~]
- [gain~]
- [gain2~]
- [button]
- [keyboard]
- [graph~]
- [range.hsl]
- [multi.vsl]
- [spectrograph~]
- [meter~]
- [meter2~]
- [meter4~]
- [meter8~]
- [note]
- [mix2~]
- [mix4~]
- [setdsp~]
- [openfile]
- [oscilloscope~]
