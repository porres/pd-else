############
# Makefile #
############

# library name

lib.name = else

cflags = -Ishared -DHAVE_STRUCT_TIMESPEC

uname := $(shell uname -s)

#########################################################################
# sources
#########################################################################

# envgen~.class.sources := classes/source/envgen~.c

# GUI:
function.class.sources := classes/source/function.c
keyboard.class.sources := classes/source/keyboard.c
openfile.class.sources := classes/source/openfile.c
colors.class.sources := classes/source/colors.c
pic.class.sources := classes/source/pic.c

# control:
ctl.in.class.sources := classes/source/ctl.in.c
ctl.out.class.sources := classes/source/ctl.out.c
suspedal.class.sources := classes/source/suspedal.c
voices.class.sources := classes/source/voices.c
args.class.sources := classes/source/args.c
click.class.sources := classes/source/click.c
properties.class.sources := classes/source/properties.c
break.class.sources := classes/source/break.c
cents2ratio.class.sources := classes/source/cents2ratio.c
changed.class.sources := classes/source/changed.c
coin.class.sources := classes/source/coin.c
dir.class.sources := classes/source/dir.c
hot.class.sources := classes/source/hot.c
hz2rad.class.sources := classes/source/hz2rad.c
initmess.class.sources := classes/source/initmess.c
lb.class.sources := classes/source/lb.c
loadbanger.class.sources := classes/source/loadbanger.c
nbang.class.sources := classes/source/nbang.c
note.in.class.sources := classes/source/note.in.c
note.out.class.sources := classes/source/note.out.c
float2bits.class.sources := classes/source/float2bits.c
panic.class.sources := classes/source/panic.c
pgm.in.class.sources := classes/source/pgm.in.c
pgm.out.class.sources := classes/source/pgm.out.c
bend.in.class.sources := classes/source/bend.in.c
bend.out.class.sources := classes/source/bend.out.c
pack2.class.sources := classes/source/pack2.c
quantizer.class.sources := classes/source/quantizer.c
randf.class.sources := classes/source/randf.c
randi.class.sources := classes/source/randi.c
rad2hz.class.sources := classes/source/rad2hz.c
ratio2cents.class.sources := classes/source/ratio2cents.c
rescale.class.sources := classes/source/rescale.c
rint.class.sources := classes/source/rint.c
routeall.class.sources := classes/source/routeall.c
routetype.class.sources := classes/source/routetype.c
fromany.class.sources := classes/source/fromany.c
toany.class.sources := classes/source/toany.c
touch.in.class.sources := classes/source/touch.in.c
touch.out.class.sources := classes/source/touch.out.c

# signal:
add~.class.sources := classes/source/add~.c
allpass.2nd~.class.sources := classes/source/allpass.2nd~.c
allpass.rev~.class.sources := classes/source/allpass.rev~.c
comb.rev~.class.sources := classes/source/comb.rev~.c
comb.filt~.class.sources := classes/source/comb.filt~.c
adsr~.class.sources := classes/source/adsr~.c
asr~.class.sources := classes/source/asr~.c
autofade~.class.sources := classes/source/autofade~.c
autofade2~.class.sources := classes/source/autofade2~.c
balance~.class.sources := classes/source/balance~.c
bandpass~.class.sources := classes/source/bandpass~.c
bandstop~.class.sources := classes/source/bandstop~.c
blocksize~.class.sources := classes/source/blocksize~.c
bicoeff.class.sources := classes/source/bicoeff.c
biquads~.class.sources := classes/source/biquads~.c
ceil.class.sources := classes/source/ceil.c
ceil~.class.sources := classes/source/ceil~.c
cents2ratio~.class.sources := classes/source/cents2ratio~.c
coin~.class.sources := classes/source/coin~.c
changed~.class.sources := classes/source/changed~.c
changed2~.class.sources := classes/source/changed2~.c
crackle~.class.sources := classes/source/crackle~.c
crossover~.class.sources := classes/source/crossover~.c
cusp~.class.sources := classes/source/cusp~.c
decay~.class.sources := classes/source/decay~.c
decay2~.class.sources := classes/source/decay2~.c
downsample~.class.sources := classes/source/downsample~.c
drive~.class.sources := classes/source/drive~.c
dust~.class.sources := classes/source/dust~.c
dust2~.class.sources := classes/source/dust2~.c
detect~.class.sources := classes/source/detect~.c
envgen~.class.sources := classes/source/envgen~.c
eq~.class.sources := classes/source/eq~.c
fader~.class.sources := classes/source/fader~.c
fbdelay~.class.sources := classes/source/fbdelay~.c
ffdelay~.class.sources := classes/source/ffdelay~.c
fbsine2~.class.sources := classes/source/fbsine2~.c
float2sig~.class.sources := classes/source/float2sig~.c
f2s~.class.sources := classes/source/f2s~.c
floor.class.sources := classes/source/floor.c
floor~.class.sources := classes/source/floor~.c
fold.class.sources := classes/source/fold.c
fold~.class.sources := classes/source/fold~.c
freq.shift~.class.sources := classes/source/freq.shift~.c
function~.class.sources := classes/source/function~.c
gbman~.class.sources := classes/source/gbman~.c
# gate~.class.sources := classes/source/gate~.c
gate2imp~.class.sources := classes/source/gate2imp~.c
glide~.class.sources := classes/source/glide~.c
glide2~.class.sources := classes/source/glide2~.c
henon~.class.sources := classes/source/henon~.c
highpass~.class.sources := classes/source/highpass~.c
highshelf~.class.sources := classes/source/highshelf~.c
hz2rad~.class.sources := classes/source/hz2rad~.c
ikeda~.class.sources := classes/source/ikeda~.c
impseq~.class.sources := classes/source/impseq~.c
int~.class.sources := classes/source/int~.c
lastvalue~.class.sources := classes/source/lastvalue~.c
latoocarfian~.class.sources := classes/source/latoocarfian~.c
lorenz~.class.sources := classes/source/lorenz~.c
lfnoise~.class.sources := classes/source/lfnoise~.c
lincong~.class.sources := classes/source/lincong~.c
logistic~.class.sources := classes/source/logistic~.c
loop.class.sources := classes/source/loop.c
lowpass~.class.sources := classes/source/lowpass~.c
lowshelf~.class.sources := classes/source/lowshelf~.c
mov.rms~.class.sources := classes/source/mov.rms~.c
mtx~.class.sources := classes/source/mtx~.c
match~.class.sources := classes/source/match~.c
mov.avg~.class.sources := classes/source/mov.avg~.c
median~.class.sources := classes/source/median~.c
tempo~.class.sources := classes/source/tempo~.c
nyquist~.class.sources := classes/source/nyquist~.c
pan2~.class.sources := classes/source/pan2~.c
pan4~.class.sources := classes/source/pan4~.c
peak~.class.sources := classes/source/peak~.c
pmosc~.class.sources := classes/source/pmosc~.c
pulsecount~.class.sources := classes/source/pulsecount~.c
pulsediv~.class.sources := classes/source/pulsediv~.c
quad~.class.sources := classes/source/quad~.c
quantizer~.class.sources := classes/source/quantizer~.c
rad2hz~.class.sources := classes/source/rad2hz~.c
ramp~.class.sources := classes/source/ramp~.c
rampnoise~.class.sources := classes/source/rampnoise~.c
randf~.class.sources := classes/source/randf~.c
randi~.class.sources := classes/source/randi~.c
randpulse~.class.sources := classes/source/randpulse~.c
randpulse2~.class.sources := classes/source/randpulse2~.c
ratio2cents~.class.sources := classes/source/ratio2cents~.c
rescale~.class.sources := classes/source/rescale~.c
rint~.class.sources := classes/source/rint~.c
resonant~.class.sources := classes/source/resonant~.c
resonant2~.class.sources := classes/source/resonant2~.c
rms~.class.sources := classes/source/rms~.c
rotate~.class.sources := classes/source/rotate~.c
sh~.class.sources := classes/source/sh~.c
schmitt~.class.sources := classes/source/schmitt~.c
stepnoise~.class.sources := classes/source/stepnoise~.c
sin~.class.sources := classes/source/sin~.c
sig2float~.class.sources := classes/source/sig2float~.c
s2f~.class.sources := classes/source/s2f~.c
sequencer~.class.sources := classes/source/sequencer~.c
sr~.class.sources := classes/source/sr~.c
status~.class.sources := classes/source/status~.c
standard~.class.sources := classes/source/standard~.c
susloop~.class.sources := classes/source/susloop~.c
svfilter~.class.sources := classes/source/svfilter~.c
trig.delay~.class.sources := classes/source/trig.delay~.c
trig.delay2~.class.sources := classes/source/trig.delay2~.c
timed.gate~.class.sources := classes/source/timed.gate~.c
toggleff~.class.sources := classes/source/toggleff~.c
trighold~.class.sources := classes/source/trighold~.c
vu~.class.sources := classes/source/vu~.c
xfade~.class.sources := classes/source/xfade~.c
xgate~.class.sources := classes/source/xgate~.c
xgate2~.class.sources := classes/source/xgate2~.c
xmod~.class.sources := classes/source/xmod~.c
xselect~.class.sources := classes/source/xselect~.c
xselect2~.class.sources := classes/source/xselect2~.c
wrap2.class.sources := classes/source/wrap2.c
wrap2~.class.sources := classes/source/wrap2~.c
zerocross~.class.sources := classes/source/zerocross~.c

# dependencies:

magic := shared/magic.c
    sine~.class.sources := classes/source/sine~.c $(magic)
    cosine~.class.sources := classes/source/cosine~.c $(magic)
    fbsine~.class.sources := classes/source/fbsine~.c $(magic)
    imp~.class.sources := classes/source/imp~.c $(magic)
    impulse~.class.sources := classes/source/impulse~.c $(magic)
    imp2~.class.sources := classes/source/imp2~.c $(magic)
    impulse2~.class.sources := classes/source/impulse2~.c $(magic)
    parabolic~.class.sources := classes/source/parabolic~.c $(magic)
    pulse~.class.sources := classes/source/pulse~.c $(magic)
    saw~.class.sources := classes/source/saw~.c $(magic)
    saw2~.class.sources := classes/source/saw2~.c $(magic)
    square~.class.sources := classes/source/square~.c $(magic)
    tri~.class.sources := classes/source/tri~.c $(magic)
    vsaw~.class.sources := classes/source/vsaw~.c $(magic)
    pimp~.class.sources := classes/source/pimp~.c $(magic)

buf := shared/buffer.c
    shaper~.class.sources = classes/source/shaper~.c $(buf)
    table~.class.sources = classes/source/table~.c $(buf)

bufmagic := \
shared/magic.c \
shared/buffer.c
    wavetable~.class.sources = classes/source/wavetable~.c $(bufmagic)
    wt~.class.sources = classes/source/wt~.c $(bufmagic)

gui := shared/gui.c
    window.class.sources := classes/source/window.c $(gui)

noise := shared/random.c
    brown~.class.sources := classes/source/brown~.c $(noise)
    gray~.class.sources := classes/source/gray~.c $(noise)
    pinknoise~.class.sources := classes/source/pinknoise~.c $(noise)
    clipnoise~.class.sources := classes/source/clipnoise~.c $(noise)
    pluck~.class.sources := classes/source/pluck~.c $(noise)

midi := \
    shared/mifi.c \
    shared/file.c \
    shared/grow.c
    midi.class.sources := classes/source/midi.c $(midi)

#########################################################################

# extra files

datafiles = \
$(wildcard classes/abstractions/*.pd) \
$(wildcard help/*.pd) \
$(wildcard help/*.wav) \
$(wildcard help/*.gif) \
$(wildcard help/*.mid) \
$(wildcard *.txt) \
else-meta.pd \
README.pdf

#########################################################################

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
