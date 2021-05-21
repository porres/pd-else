############
# Makefile #
############

# library name

lib.name = else

cflags = -Ishared -DHAVE_STRUCT_TIMESPEC

uname := $(shell uname -s)

#########################################################################
# Sources: ##############################################################
#########################################################################

# Lib:
else.class.sources := Classes/Source/else.c

# EXTRA:
cmul~.class.sources := Classes/Source/cmul~.c

# GUI:
button.class.sources := Classes/Source/button.c
pic.class.sources := Classes/Source/pic.c
keyboard.class.sources := Classes/Source/keyboard.c
function.class.sources := Classes/Source/function.c
pad.class.sources := Classes/Source/pad.c
openfile.class.sources := Classes/Source/openfile.c
colors.class.sources := Classes/Source/colors.c
gui.class.sources := Classes/Source/gui.c

# control:
args.class.sources := Classes/Source/args.c
ctl.in.class.sources := Classes/Source/ctl.in.c
ctl.out.class.sources := Classes/Source/ctl.out.c
suspedal.class.sources := Classes/Source/suspedal.c
voices.class.sources := Classes/Source/voices.c
buffer.class.sources := Classes/Source/buffer_obj.c
click.class.sources := Classes/Source/click.c
canvas.active.class.sources := Classes/Source/canvas.active.c
canvas.bounds.class.sources := Classes/Source/canvas.bounds.c
canvas.edit.class.sources := Classes/Source/canvas.edit.c
canvas.gop.class.sources := Classes/Source/canvas.gop.c
canvas.mouse.class.sources := Classes/Source/canvas.mouse.c
canvas.pos.class.sources := Classes/Source/canvas.pos.c
canvas.setname.class.sources := Classes/Source/canvas.setname.c
canvas.name.class.sources := Classes/Source/canvas.name.c
canvas.wname.class.sources := Classes/Source/canvas.wname.c
canvas.vis.class.sources := Classes/Source/canvas.vis.c
canvas.zoom.class.sources := Classes/Source/canvas.zoom.c
properties.class.sources := Classes/Source/properties.c
break.class.sources := Classes/Source/break.c
cents2ratio.class.sources := Classes/Source/cents2ratio.c
changed.class.sources := Classes/Source/changed.c
gcd.class.sources := Classes/Source/gcd.c
chance.class.sources := Classes/Source/chance.c
dir.class.sources := Classes/Source/dir.c
spread.class.sources := Classes/Source/spread.c
dollsym.class.sources := Classes/Source/dollsym.c
hot.class.sources := Classes/Source/hot.c
hz2rad.class.sources := Classes/Source/hz2rad.c
initmess.class.sources := Classes/Source/initmess.c
lb.class.sources := Classes/Aliases/lb.c
limit.class.sources := Classes/Source/limit.c
loadbanger.class.sources := Classes/Source/loadbanger.c
merge.class.sources := Classes/Source/merge.c
fontsize.class.sources := Classes/Source/fontsize.c
format.class.sources := Classes/Source/format.c
message.class.sources := Classes/Source/message.c
messbox.class.sources := Classes/Source/messbox.c
mouse.class.sources := Classes/Source/mouse.c
note.in.class.sources := Classes/Source/note.in.c
note.out.class.sources := Classes/Source/note.out.c
factor.class.sources := Classes/Source/factor.c
float2bits.class.sources := Classes/Source/float2bits.c
panic.class.sources := Classes/Source/panic.c
pgm.in.class.sources := Classes/Source/pgm.in.c
pgm.out.class.sources := Classes/Source/pgm.out.c
bend.in.class.sources := Classes/Source/bend.in.c
bend.out.class.sources := Classes/Source/bend.out.c
pack2.class.sources := Classes/Source/pack2.c
quantizer.class.sources := Classes/Source/quantizer.c
rand.f.class.sources := Classes/Source/rand.f.c
rand.i.class.sources := Classes/Source/rand.i.c
rand.seq.class.sources := Classes/Source/rand.seq.c
rad2hz.class.sources := Classes/Source/rad2hz.c
ratio2cents.class.sources := Classes/Source/ratio2cents.c
rescale.class.sources := Classes/Source/rescale.c
rint.class.sources := Classes/Source/rint.c
router.class.sources := Classes/Source/router.c
routeall.class.sources := Classes/Source/routeall.c
routetype.class.sources := Classes/Source/routetype.c
receiver.class.sources := Classes/Source/receiver.c
retrieve.class.sources := Classes/Source/retrieve.c
selector.class.sources := Classes/Source/selector.c
separate.class.sources := Classes/Source/separate.c
symbol2any.class.sources := Classes/Source/symbol2any.c
slice.class.sources := Classes/Source/slice.c
sort.class.sources := Classes/Source/sort.c
store.class.sources := Classes/Source/store.c
touch.in.class.sources := Classes/Source/touch.in.c
touch.out.class.sources := Classes/Source/touch.out.c
trigger2.class.sources := Classes/Source/trigger2.c
t2.class.sources := Classes/Aliases/t2.c
unmerge.class.sources := Classes/Source/unmerge.c

# signal:
above~.class.sources := Classes/Source/above~.c
add~.class.sources := Classes/Source/add~.c
allpass.2nd~.class.sources := Classes/Source/allpass.2nd~.c
allpass.rev~.class.sources := Classes/Source/allpass.rev~.c
comb.rev~.class.sources := Classes/Source/comb.rev~.c
comb.filt~.class.sources := Classes/Source/comb.filt~.c
adsr~.class.sources := Classes/Source/adsr~.c
asr~.class.sources := Classes/Source/asr~.c
autofade~.class.sources := Classes/Source/autofade~.c
autofade2~.class.sources := Classes/Source/autofade2~.c
balance~.class.sources := Classes/Source/balance~.c
bandpass~.class.sources := Classes/Source/bandpass~.c
bandstop~.class.sources := Classes/Source/bandstop~.c
blocksize~.class.sources := Classes/Source/blocksize~.c
bicoeff.class.sources := Classes/Source/bicoeff.c
biquads~.class.sources := Classes/Source/biquads~.c
ceil.class.sources := Classes/Source/ceil.c
ceil~.class.sources := Classes/Source/ceil~.c
cents2ratio~.class.sources := Classes/Source/cents2ratio~.c
chance~.class.sources := Classes/Source/chance~.c
changed~.class.sources := Classes/Source/changed~.c
changed2~.class.sources := Classes/Source/changed2~.c
crackle~.class.sources := Classes/Source/crackle~.c
crossover~.class.sources := Classes/Source/crossover~.c
cusp~.class.sources := Classes/Source/cusp~.c
decay~.class.sources := Classes/Source/decay~.c
decay2~.class.sources := Classes/Source/decay2~.c
del~.class.sources := Classes/Source/del~.c
downsample~.class.sources := Classes/Source/downsample~.c
drive~.class.sources := Classes/Source/drive~.c
dust~.class.sources := Classes/Source/dust~.c
dust2~.class.sources := Classes/Source/dust2~.c
detect~.class.sources := Classes/Source/detect~.c
envgen~.class.sources := Classes/Source/envgen~.c
eq~.class.sources := Classes/Source/eq~.c
fader~.class.sources := Classes/Source/fader~.c
fbdelay~.class.sources := Classes/Source/fbdelay~.c
ffdelay~.class.sources := Classes/Source/ffdelay~.c
fbsine2~.class.sources := Classes/Source/fbsine2~.c
float2sig~.class.sources := Classes/Source/float2sig~.c
f2s~.class.sources := Classes/Aliases/f2s~.c
fdn.rev~.class.sources := Classes/Source/fdn.rev~.c
floor.class.sources := Classes/Source/floor.c
floor~.class.sources := Classes/Source/floor~.c
fold.class.sources := Classes/Source/fold.c
fold~.class.sources := Classes/Source/fold~.c
freq.shift~.class.sources := Classes/Source/freq.shift~.c
function~.class.sources := Classes/Source/function~.c
gbman~.class.sources := Classes/Source/gbman~.c
gate2imp~.class.sources := Classes/Source/gate2imp~.c
giga.rev~.class.sources := Classes/Source/giga.rev~.c
glide~.class.sources := Classes/Source/glide~.c
glide2~.class.sources := Classes/Source/glide2~.c
henon~.class.sources := Classes/Source/henon~.c
highpass~.class.sources := Classes/Source/highpass~.c
highshelf~.class.sources := Classes/Source/highshelf~.c
hz2rad~.class.sources := Classes/Source/hz2rad~.c
ikeda~.class.sources := Classes/Source/ikeda~.c
impseq~.class.sources := Classes/Source/impseq~.c
trunc~.class.sources := Classes/Source/trunc~.c
trunc.class.sources := Classes/Source/trunc.c
lastvalue~.class.sources := Classes/Source/lastvalue~.c
latoocarfian~.class.sources := Classes/Source/latoocarfian~.c
lorenz~.class.sources := Classes/Source/lorenz~.c
lfnoise~.class.sources := Classes/Source/lfnoise~.c
lincong~.class.sources := Classes/Source/lincong~.c
logistic~.class.sources := Classes/Source/logistic~.c
loop.class.sources := Classes/Source/loop.c
lowpass~.class.sources := Classes/Source/lowpass~.c
lowshelf~.class.sources := Classes/Source/lowshelf~.c
mov.rms~.class.sources := Classes/Source/mov.rms~.c
mtx~.class.sources := Classes/Source/mtx~.c
match~.class.sources := Classes/Source/match~.c
mov.avg~.class.sources := Classes/Source/mov.avg~.c
median~.class.sources := Classes/Source/median~.c
tempo~.class.sources := Classes/Source/tempo~.c
nyquist~.class.sources := Classes/Source/nyquist~.c
op~.class.sources := Classes/Source/op~.c
power~.class.sources := Classes/Source/power~.c
pan2~.class.sources := Classes/Source/pan2~.c
pan4~.class.sources := Classes/Source/pan4~.c
peak~.class.sources := Classes/Source/peak~.c
pmosc~.class.sources := Classes/Source/pmosc~.c
pulsecount~.class.sources := Classes/Source/pulsecount~.c
pulsediv~.class.sources := Classes/Source/pulsediv~.c
quad~.class.sources := Classes/Source/quad~.c
quantizer~.class.sources := Classes/Source/quantizer~.c
rad2hz~.class.sources := Classes/Source/rad2hz~.c
ramp~.class.sources := Classes/Source/ramp~.c
rampnoise~.class.sources := Classes/Source/rampnoise~.c
range~.class.sources := Classes/Source/range~.c
rand.f~.class.sources := Classes/Source/rand.f~.c
rand.i~.class.sources := Classes/Source/rand.i~.c
randpulse~.class.sources := Classes/Source/randpulse~.c
randpulse2~.class.sources := Classes/Source/randpulse2~.c
ratio2cents~.class.sources := Classes/Source/ratio2cents~.c
rescale~.class.sources := Classes/Source/rescale~.c
rint~.class.sources := Classes/Source/rint~.c
resonant~.class.sources := Classes/Source/resonant~.c
resonant2~.class.sources := Classes/Source/resonant2~.c
rms~.class.sources := Classes/Source/rms~.c
rotate~.class.sources := Classes/Source/rotate~.c
sh~.class.sources := Classes/Source/sh~.c
schmitt~.class.sources := Classes/Source/schmitt~.c
stepnoise~.class.sources := Classes/Source/stepnoise~.c
lag~.class.sources := Classes/Source/lag~.c
lag2~.class.sources := Classes/Source/lag2~.c
sin~.class.sources := Classes/Source/sin~.c
sig2float~.class.sources := Classes/Source/sig2float~.c
slew~.class.sources := Classes/Source/slew~.c
slew2~.class.sources := Classes/Source/slew2~.c
s2f~.class.sources := Classes/Aliases/s2f~.c
sequencer~.class.sources := Classes/Source/sequencer~.c
sr~.class.sources := Classes/Source/sr~.c
status~.class.sources := Classes/Source/status~.c
standard~.class.sources := Classes/Source/standard~.c
spread~.class.sources := Classes/Source/spread~.c
susloop~.class.sources := Classes/Source/susloop~.c
svfilter~.class.sources := Classes/Source/svfilter~.c
trig.delay~.class.sources := Classes/Source/trig.delay~.c
trig.delay2~.class.sources := Classes/Source/trig.delay2~.c
timed.gate~.class.sources := Classes/Source/timed.gate~.c
toggleff~.class.sources := Classes/Source/toggleff~.c
trighold~.class.sources := Classes/Source/trighold~.c
vu~.class.sources := Classes/Source/vu~.c
xfade~.class.sources := Classes/Source/xfade~.c
xgate~.class.sources := Classes/Source/xgate~.c
xgate2~.class.sources := Classes/Source/xgate2~.c
xmod~.class.sources := Classes/Source/xmod~.c
xmod2~.class.sources := Classes/Source/xmod2~.c
xselect~.class.sources := Classes/Source/xselect~.c
xselect2~.class.sources := Classes/Source/xselect2~.c
wrap2.class.sources := Classes/Source/wrap2.c
wrap2~.class.sources := Classes/Source/wrap2~.c
zerocross~.class.sources := Classes/Source/zerocross~.c

magic := shared/magic.c
    sine~.class.sources := Classes/Source/sine~.c $(magic)
    cosine~.class.sources := Classes/Source/cosine~.c $(magic)
    fbsine~.class.sources := Classes/Source/fbsine~.c $(magic)
    imp~.class.sources := Classes/Aliases/imp~.c $(magic)
    impulse~.class.sources := Classes/Source/impulse~.c $(magic)
    imp2~.class.sources := Classes/Aliases/imp2~.c $(magic)
    impulse2~.class.sources := Classes/Source/impulse2~.c $(magic)
    parabolic~.class.sources := Classes/Source/parabolic~.c $(magic)
    pulse~.class.sources := Classes/Source/pulse~.c $(magic)
    saw~.class.sources := Classes/Source/saw~.c $(magic)
    saw2~.class.sources := Classes/Source/saw2~.c $(magic)
    square~.class.sources := Classes/Source/square~.c $(magic)
    tri~.class.sources := Classes/Source/tri~.c $(magic)
    vsaw~.class.sources := Classes/Source/vsaw~.c $(magic)
    pimp~.class.sources := Classes/Source/pimp~.c $(magic)

buf := shared/buffer.c
    shaper~.class.sources = Classes/Source/shaper~.c $(buf)
    table~.class.sources = Classes/Source/table~.c $(buf)
    tabwriter~.class.sources = Classes/Source/tabwriter~.c $(buf)

bufmagic := \
shared/magic.c \
shared/buffer.c
    wavetable~.class.sources = Classes/Source/wavetable~.c $(bufmagic)
    wt~.class.sources = Classes/Aliases/wt~.c $(bufmagic)
    tabplayer~.class.sources = Classes/Source/tabplayer~.c $(bufmagic)

rand := shared/random.c
    brown~.class.sources := Classes/Source/brown~.c $(rand)
    gray~.class.sources := Classes/Source/gray~.c $(rand)
    pinknoise~.class.sources := Classes/Source/pinknoise~.c $(rand)
    clipnoise~.class.sources := Classes/Source/clipnoise~.c $(rand)
    pluck~.class.sources := Classes/Source/pluck~.c $(rand)

midi := \
    shared/mifi.c \
    shared/file.c \
    shared/grow.c
    midi.class.sources := Classes/Source/midi.c $(midi)
    
# GUI:
utf := shared/s_utf8.c
	note.class.sources := Classes/Source/note.c $(utf)

smagic := shared/magic.c
    oscilloscope~.class.sources := Classes/Source/oscilloscope.c $(smagic)

#########################################################################

# extra files

datafiles = \
$(wildcard Classes/Abstractions/*.pd) \
$(wildcard Help-files/*.pd) \
$(wildcard extra/*.pd) \
$(wildcard extra/*.aif) \
$(wildcard extra/*.wav) \
$(wildcard extra/*.gif) \
$(wildcard extra/*.mid) \
$(wildcard extra/*.txt) \
$(wildcard *.txt) \
README.pdf

#########################################################################

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
