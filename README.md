ELSE 1.0 (currently at alpha stage)

----------------------------------------------

ELSE - EL Locus Solus' Else - library for Pd

"EL Locus Solus" is run by Alexandre Torres Porres, who organizes cultural events and teaches computer music courses since around 2009, at the time of PdCon09 (the 3rd International Pure Data Convention in 2009, São Paulo - Brasil); website: http://alexandre-torres.wixsite.com/el-locus-solus

EL Locus Solus offers a computer music tutorial with examples in Pure Data for its courses. These are in portuguese as of yet, with plans for english translation and formatting into a book. There are over 350 examples for now and they were designed to work with Pd Extended 0.42-5, making extensive use of the existing objects available in Pd Extended's libraries.

Even though extended has quite a large set of external libraries and objects, at some point there was the need of something "else". Thus, EL Locus Solus is now not only offering computer music examples, but also the "else" library, with extra objects for its didactic material.

----------------

The current library state is at alpha experimental releases, where drastic changes may occur and and backwards compatibility is not guaranteed for future releases

----------------------

Object list (75 objects):

OSCILLATORS (DETERMINISTIC GENERATORS): [7]
- [imp~]
- [imp2~]
- [parabolic~]
- [pulse~]
- [sawtooth~]
- [square~]
- [vtriangle~]

CHAOTIC GENERATORS: [13]
- [brown~] 
- [clipnoise~] 
- [crackle~] 
- [cusp~] 
- [gbman~] 
- [henon~]
- [latoocarfian~]
- [lfnoise~]
- [lincong~]
- [logistic~]
- [quad~]
- [random~]
- [standard~]

CONVERSION: [16]
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

TRIGGERS: [8]
- [dust~]
- [dust2~]
- [pimp~]
- [pulsecount~]
- [pulsediv~]
- [sh~]
- [toggleff~]
- [trigate~]

CONSTANT VALUES: [4]
- [nyquist]
- [pi]
- [sr~]
- [e]

GUI: [9]
- [out~]
- [gain~]
- [gain2~]
- [graph~]
- [meter~]
- [meter2~]
- [mix2~]
- [mix4~]
- [setdsp~]

SIGNAL ROUTING: [4]
- [balance~]
- [pan2~]
- [pan4~]
- [xfade~]

MISCELANEOUS: [4]
- [accum~]
- [downsample~]
- [lastvalue]
- [sin~]
