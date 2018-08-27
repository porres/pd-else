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

# envgen~.class.sources := classes/envgen~.c

# GUI:
keyboard.class.sources := classes/keyboard.c
openfile.class.sources := classes/openfile.c
colors.class.sources := classes/colors.c
pic.class.sources := classes/pic.c

# control:
ctl.in.class.sources := classes/ctl.in.c
ctl.out.class.sources := classes/ctl.out.c
suspedal.class.sources := classes/suspedal.c
voices.class.sources := classes/voices.c
args.class.sources := classes/args.c
click.class.sources := classes/click.c
properties.class.sources := classes/properties.c
break.class.sources := classes/break.c
cents2ratio.class.sources := classes/cents2ratio.c
changed.class.sources := classes/changed.c
coin.class.sources := classes/coin.c
dir.class.sources := classes/dir.c
hot.class.sources := classes/hot.c
hz2rad.class.sources := classes/hz2rad.c
initmess.class.sources := classes/initmess.c
lb.class.sources := classes/lb.c
loadbanger.class.sources := classes/loadbanger.c
nbang.class.sources := classes/nbang.c
note.in.class.sources := classes/note.in.c
note.out.class.sources := classes/note.out.c
float2bits.class.sources := classes/float2bits.c
panic.class.sources := classes/panic.c
pgm.in.class.sources := classes/pgm.in.c
pgm.out.class.sources := classes/pgm.out.c
bend.in.class.sources := classes/bend.in.c
bend.out.class.sources := classes/bend.out.c
pack2.class.sources := classes/pack2.c
power.class.sources := classes/power.c
quantizer.class.sources := classes/quantizer.c
randf.class.sources := classes/randf.c
randi.class.sources := classes/randi.c
rad2hz.class.sources := classes/rad2hz.c
ratio2cents.class.sources := classes/ratio2cents.c
rescale.class.sources := classes/rescale.c
rint.class.sources := classes/rint.c
routeall.class.sources := classes/routeall.c
routetype.class.sources := classes/routetype.c
fromany.class.sources := classes/fromany.c
toany.class.sources := classes/toany.c
touch.in.class.sources := classes/touch.in.c
touch.out.class.sources := classes/touch.out.c


# signal:
add~.class.sources := classes/add~.c
allpass.2nd~.class.sources := classes/allpass.2nd~.c
allpass.rev~.class.sources := classes/allpass.rev~.c
adsr~.class.sources := classes/adsr~.c
asr~.class.sources := classes/asr~.c
autofade~.class.sources := classes/autofade~.c
autofade2~.class.sources := classes/autofade2~.c
balance~.class.sources := classes/balance~.c
bandpass~.class.sources := classes/bandpass~.c
bandstop~.class.sources := classes/bandstop~.c
blocksize~.class.sources := classes/blocksize~.c
bicoeff.class.sources := classes/bicoeff.c
biquads~.class.sources := classes/biquads~.c
ceil.class.sources := classes/ceil.c
ceil~.class.sources := classes/ceil~.c
cents2ratio~.class.sources := classes/cents2ratio~.c
coin~.class.sources := classes/coin~.c
changed~.class.sources := classes/changed~.c
changed2~.class.sources := classes/changed2~.c
crackle~.class.sources := classes/crackle~.c
crossover~.class.sources := classes/crossover~.c
cusp~.class.sources := classes/cusp~.c
decay~.class.sources := classes/decay~.c
decay2~.class.sources := classes/decay2~.c
downsample~.class.sources := classes/downsample~.c
drive~.class.sources := classes/drive~.c
dust~.class.sources := classes/dust~.c
dust2~.class.sources := classes/dust2~.c
elapsed~.class.sources := classes/elapsed~.c
eq~.class.sources := classes/eq~.c
fader~.class.sources := classes/fader~.c
fbdelay~.class.sources := classes/fbdelay~.c
ffdelay~.class.sources := classes/ffdelay~.c
fbsine2~.class.sources := classes/fbsine2~.c
float2sig~.class.sources := classes/float2sig~.c
f2s~.class.sources := classes/f2s~.c
floor.class.sources := classes/floor.c
floor~.class.sources := classes/floor~.c
fold.class.sources := classes/fold.c
fold~.class.sources := classes/fold~.c
freqshift~.class.sources := classes/freqshift~.c
gbman~.class.sources := classes/gbman~.c
# gate~.class.sources := classes/gate~.c
gate2imp~.class.sources := classes/gate2imp~.c
glide~.class.sources := classes/glide~.c
glide2~.class.sources := classes/glide2~.c
henon~.class.sources := classes/henon~.c
highpass~.class.sources := classes/highpass~.c
highshelf~.class.sources := classes/highshelf~.c
hz2rad~.class.sources := classes/hz2rad~.c
ikeda~.class.sources := classes/ikeda~.c
impseq~.class.sources := classes/impseq~.c
int~.class.sources := classes/int~.c
lastvalue~.class.sources := classes/lastvalue~.c
latoocarfian~.class.sources := classes/latoocarfian~.c
lorenz~.class.sources := classes/lorenz~.c
lfnoise~.class.sources := classes/lfnoise~.c
lincong~.class.sources := classes/lincong~.c
logistic~.class.sources := classes/logistic~.c
loop.class.sources := classes/loop.c
lowpass~.class.sources := classes/lowpass~.c
lowshelf~.class.sources := classes/lowshelf~.c
mov.rms~.class.sources := classes/mov.rms~.c
mtx~.class.sources := classes/mtx~.c
match~.class.sources := classes/match~.c
mov.avg~.class.sources := classes/mov.avg~.c
median~.class.sources := classes/median~.c
tempo~.class.sources := classes/tempo~.c
nyquist~.class.sources := classes/nyquist~.c
pan2~.class.sources := classes/pan2~.c
pan4~.class.sources := classes/pan4~.c
peak~.class.sources := classes/peak~.c
pluck~.class.sources := classes/pluck~.c
power~.class.sources := classes/power~.c
pmosc~.class.sources := classes/pmosc~.c
pulsecount~.class.sources := classes/pulsecount~.c
pulsediv~.class.sources := classes/pulsediv~.c
quad~.class.sources := classes/quad~.c
quantizer~.class.sources := classes/quantizer~.c
rad2hz~.class.sources := classes/rad2hz~.c
ramp~.class.sources := classes/ramp~.c
rampnoise~.class.sources := classes/rampnoise~.c
randf~.class.sources := classes/randf~.c
randi~.class.sources := classes/randi~.c
randpulse~.class.sources := classes/randpulse~.c
randpulse2~.class.sources := classes/randpulse2~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
rescale~.class.sources := classes/rescale~.c
rint~.class.sources := classes/rint~.c
resonant~.class.sources := classes/resonant~.c
resonant2~.class.sources := classes/resonant2~.c
rms~.class.sources := classes/rms~.c
rotate~.class.sources := classes/rotate~.c
sh~.class.sources := classes/sh~.c
schmitt~.class.sources := classes/schmitt~.c
stepnoise~.class.sources := classes/stepnoise~.c
sin~.class.sources := classes/sin~.c
sequencer~.class.sources := classes/sequencer~.c
# select~.class.sources := classes/select~.c
sr~.class.sources := classes/sr~.c
status~.class.sources := classes/status~.c
standard~.class.sources := classes/standard~.c
susloop~.class.sources := classes/susloop~.c
svfilter~.class.sources := classes/svfilter~.c
trig.delay~.class.sources := classes/trig.delay~.c
trig.delay2~.class.sources := classes/trig.delay2~.c
timed.gate~.class.sources := classes/timed.gate~.c
toggleff~.class.sources := classes/toggleff~.c
trighold~.class.sources := classes/trighold~.c
vu~.class.sources := classes/vu~.c
xfade~.class.sources := classes/xfade~.c
xgate~.class.sources := classes/xgate~.c
xgate2~.class.sources := classes/xgate2~.c
xmod~.class.sources := classes/xmod~.c
xselect~.class.sources := classes/xselect~.c
xselect2~.class.sources := classes/xselect2~.c
wrap2.class.sources := classes/wrap2.c
wrap2~.class.sources := classes/wrap2~.c
zerocross~.class.sources := classes/zerocross~.c

# dependencies:

magic := shared/magic.c
    sine~.class.sources := classes/sine~.c $(magic)
    cosine~.class.sources := classes/cosine~.c $(magic)
    fbsine~.class.sources := classes/fbsine~.c $(magic)
    imp~.class.sources := classes/imp~.c $(magic)
    impulse~.class.sources := classes/impulse~.c $(magic)
    imp2~.class.sources := classes/imp2~.c $(magic)
    impulse2~.class.sources := classes/impulse2~.c $(magic)
    parabolic~.class.sources := classes/parabolic~.c $(magic)
    pulse~.class.sources := classes/pulse~.c $(magic)
    saw~.class.sources := classes/saw~.c $(magic)
    saw2~.class.sources := classes/saw2~.c $(magic)
    square~.class.sources := classes/square~.c $(magic)
    tri~.class.sources := classes/tri~.c $(magic)
    vsaw~.class.sources := classes/vsaw~.c $(magic)
    pimp~.class.sources := classes/pimp~.c $(magic)

buf := shared/buffer.c
    shaper~.class.sources = classes/shaper~.c $(buf)
    table~.class.sources = classes/table~.c $(buf)

bufmagic := \
shared/magic.c \
shared/buffer.c
    wavetable~.class.sources = classes/wavetable~.c $(bufmagic)
    wt~.class.sources = classes/wt~.c $(bufmagic)

gui := shared/gui.c
    window.class.sources := classes/window.c $(gui)

noise := shared/random.c
    brown~.class.sources := classes/brown~.c $(noise)
    gray~.class.sources := classes/gray~.c $(noise)
    pinknoise~.class.sources := classes/pinknoise~.c $(noise)
    clipnoise~.class.sources := classes/clipnoise~.c $(noise)

midi := \
    shared/mifi.c \
    shared/file.c \
    shared/grow.c
    midi.class.sources := classes/midi.c $(midi)

#########################################################################

# extra files

datafiles = \
$(wildcard help/*.pd) \
$(wildcard help/Abstractions/*.pd) \
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
