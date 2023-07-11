############
# Makefile #
############

# library name

lib.name = else

aubioflags = -ICode_source/shared/aubio/src

define forDarwin
# old pdlibbuilder in plaits~ gets the target architecture(s) wrong
plaitsflags = arch="$(target.arch)"
endef

cflags = -ICode_source/shared -DHAVE_STRUCT_TIMESPEC $(aubioflags)

uname := $(shell uname -s)

#########################################################################
# Sources: ##############################################################
#########################################################################

# Lib:
else.class.sources := Code_source/Compiled/else.c

# GUI:
knob.class.sources := Code_source/Compiled/knob.c
button.class.sources := Code_source/Compiled/button.c
pic.class.sources := Code_source/Compiled/pic.c
keyboard.class.sources := Code_source/Compiled/keyboard.c
pad.class.sources := Code_source/Compiled/pad.c
openfile.class.sources := Code_source/Compiled/openfile.c
colors.class.sources := Code_source/Compiled/colors.c

# control:
args.class.sources := Code_source/Compiled/args.c
ctl.in.class.sources := Code_source/Compiled/ctl.in.c
ctl.out.class.sources := Code_source/Compiled/ctl.out.c
ceil.class.sources := Code_source/Compiled/ceil.c
suspedal.class.sources := Code_source/Compiled/suspedal.c
voices.class.sources := Code_source/Compiled/voices.c
buffer.class.sources := Code_source/Compiled/buffer_obj.c
bicoeff.class.sources := Code_source/Compiled/bicoeff.c
bicoeff2.class.sources := Code_source/Compiled/bicoeff2.c
click.class.sources := Code_source/Compiled/click.c
canvas.active.class.sources := Code_source/Compiled/canvas.active.c
canvas.bounds.class.sources := Code_source/Compiled/canvas.bounds.c
canvas.edit.class.sources := Code_source/Compiled/canvas.edit.c
canvas.file.class.sources := Code_source/Compiled/canvas.file.c
canvas.gop.class.sources := Code_source/Compiled/canvas.gop.c
canvas.mouse.class.sources := Code_source/Compiled/canvas.mouse.c
canvas.name.class.sources := Code_source/Compiled/canvas.name.c
canvas.pos.class.sources := Code_source/Compiled/canvas.pos.c
canvas.setname.class.sources := Code_source/Compiled/canvas.setname.c
canvas.vis.class.sources := Code_source/Compiled/canvas.vis.c
canvas.zoom.class.sources := Code_source/Compiled/canvas.zoom.c
properties.class.sources := Code_source/Compiled/properties.c
break.class.sources := Code_source/Compiled/break.c
cents2ratio.class.sources := Code_source/Compiled/cents2ratio.c
changed.class.sources := Code_source/Compiled/changed.c
gcd.class.sources := Code_source/Compiled/gcd.c
dir.class.sources := Code_source/Compiled/dir.c
datetime.class.sources := Code_source/Compiled/datetime.c
default.class.sources := Code_source/Compiled/default.c
dollsym.class.sources := Code_source/Compiled/dollsym.c
floor.class.sources := Code_source/Compiled/floor.c
fold.class.sources := Code_source/Compiled/fold.c
hot.class.sources := Code_source/Compiled/hot.c
hz2rad.class.sources := Code_source/Compiled/hz2rad.c
initmess.class.sources := Code_source/Compiled/initmess.c
keycode.class.sources := Code_source/Compiled/keycode.c
lb.class.sources := Code_source/extra_source/Aliases/lb.c
limit.class.sources := Code_source/Compiled/limit.c
loadbanger.class.sources := Code_source/Compiled/loadbanger.c
merge.class.sources := Code_source/Compiled/merge.c
fontsize.class.sources := Code_source/Compiled/fontsize.c
format.class.sources := Code_source/Compiled/format.c
message.class.sources := Code_source/Compiled/message.c
messbox.class.sources := Code_source/Compiled/messbox.c
metronome.class.sources := Code_source/Compiled/metronome.c
mouse.class.sources := Code_source/Compiled/mouse.c
noteinfo.class.sources := Code_source/Compiled/noteinfo.c
note.in.class.sources := Code_source/Compiled/note.in.c
note.out.class.sources := Code_source/Compiled/note.out.c
osc.route.class.sources := Code_source/Compiled/osc.route.c
osc.parse.class.sources := Code_source/Compiled/osc.parse.c
osc.format.class.sources := Code_source/Compiled/osc.format.c
factor.class.sources := Code_source/Compiled/factor.c
float2bits.class.sources := Code_source/Compiled/float2bits.c
panic.class.sources := Code_source/Compiled/panic.c
pgm.in.class.sources := Code_source/Compiled/pgm.in.c
pgm.out.class.sources := Code_source/Compiled/pgm.out.c
pipe2.class.sources := Code_source/Compiled/pipe2.c
bend.in.class.sources := Code_source/Compiled/bend.in.c
bend.out.class.sources := Code_source/Compiled/bend.out.c
pack2.class.sources := Code_source/Compiled/pack2.c
quantizer.class.sources := Code_source/Compiled/quantizer.c
rad2hz.class.sources := Code_source/Compiled/rad2hz.c
ratio2cents.class.sources := Code_source/Compiled/ratio2cents.c
rescale.class.sources := Code_source/Compiled/rescale.c
oldrescale.class.sources := Code_source/Compiled/oldrescale.c
rint.class.sources := Code_source/Compiled/rint.c
router.class.sources := Code_source/Compiled/router.c
route2.class.sources := Code_source/Compiled/route2.c
routeall.class.sources := Code_source/Compiled/routeall.c
routetype.class.sources := Code_source/Compiled/routetype.c
receiver.class.sources := Code_source/Compiled/receiver.c
retrieve.class.sources := Code_source/Compiled/retrieve.c
selector.class.sources := Code_source/Compiled/selector.c
separate.class.sources := Code_source/Compiled/separate.c
symbol2any.class.sources := Code_source/Compiled/symbol2any.c
slice.class.sources := Code_source/Compiled/slice.c
sort.class.sources := Code_source/Compiled/sort.c
spread.class.sources := Code_source/Compiled/spread.c
touch.in.class.sources := Code_source/Compiled/touch.in.c
touch.out.class.sources := Code_source/Compiled/touch.out.c
trunc.class.sources := Code_source/Compiled/trunc.c
unmerge.class.sources := Code_source/Compiled/unmerge.c

# signal:
above~.class.sources := Code_source/Compiled/above~.c
add~.class.sources := Code_source/Compiled/add~.c
allpass.2nd~.class.sources := Code_source/Compiled/allpass.2nd~.c
allpass.rev~.class.sources := Code_source/Compiled/allpass.rev~.c
bitnormal~.class.sources := Code_source/Compiled/bitnormal~.c
comb.rev~.class.sources := Code_source/Compiled/comb.rev~.c
comb.filt~.class.sources := Code_source/Compiled/comb.filt~.c
adsr~.class.sources := Code_source/Compiled/adsr~.c
asr~.class.sources := Code_source/Compiled/asr~.c
autofade~.class.sources := Code_source/Compiled/autofade~.c
autofade2~.class.sources := Code_source/Compiled/autofade2~.c
balance~.class.sources := Code_source/Compiled/balance~.c
bandpass~.class.sources := Code_source/Compiled/bandpass~.c
bandstop~.class.sources := Code_source/Compiled/bandstop~.c
bl.imp~.class.sources := Code_source/Compiled/bl.imp~.c
bl.imp2~.class.sources := Code_source/Compiled/bl.imp2~.c
bl.saw~.class.sources := Code_source/Compiled/bl.saw~.c
bl.saw2~.class.sources := Code_source/Compiled/bl.saw2~.c
bl.square~.class.sources := Code_source/Compiled/bl.square~.c
bl.tri~.class.sources := Code_source/Compiled/bl.tri~.c
bl.vsaw~.class.sources := Code_source/Compiled/bl.vsaw~.c
blocksize~.class.sources := Code_source/Compiled/blocksize~.c
biquads~.class.sources := Code_source/Compiled/biquads~.c
car2pol~.class.sources := Code_source/Compiled/car2pol~.c
ceil~.class.sources := Code_source/Compiled/ceil~.c
cents2ratio~.class.sources := Code_source/Compiled/cents2ratio~.c
changed~.class.sources := Code_source/Compiled/changed~.c
changed2~.class.sources := Code_source/Compiled/changed2~.c
cmul~.class.sources := Code_source/Compiled/cmul~.c
crackle~.class.sources := Code_source/Compiled/crackle~.c
crossover~.class.sources := Code_source/Compiled/crossover~.c
cusp~.class.sources := Code_source/Compiled/cusp~.c
db2lin~.class.sources := Code_source/Compiled/db2lin~.c
decay~.class.sources := Code_source/Compiled/decay~.c
decay2~.class.sources := Code_source/Compiled/decay2~.c
downsample~.class.sources := Code_source/Compiled/downsample~.c
drive~.class.sources := Code_source/Compiled/drive~.c
detect~.class.sources := Code_source/Compiled/detect~.c
envgen~.class.sources := Code_source/Compiled/envgen~.c
eq~.class.sources := Code_source/Compiled/eq~.c
fader~.class.sources := Code_source/Compiled/fader~.c
fbsine2~.class.sources := Code_source/Compiled/fbsine2~.c
float2sig~.class.sources := Code_source/Compiled/float2sig~.c
f2s~.class.sources := Code_source/extra_source/Aliases/f2s~.c
fdn.rev~.class.sources := Code_source/Compiled/fdn.rev~.c
floor~.class.sources := Code_source/Compiled/floor~.c
fold~.class.sources := Code_source/Compiled/fold~.c
freq.shift~.class.sources := Code_source/Compiled/freq.shift~.c
gbman~.class.sources := Code_source/Compiled/gbman~.c
gate2imp~.class.sources := Code_source/Compiled/gate2imp~.c
giga.rev~.class.sources := Code_source/Compiled/giga.rev~.c
glide~.class.sources := Code_source/Compiled/glide~.c
glide2~.class.sources := Code_source/Compiled/glide2~.c
henon~.class.sources := Code_source/Compiled/henon~.c
highpass~.class.sources := Code_source/Compiled/highpass~.c
highshelf~.class.sources := Code_source/Compiled/highshelf~.c
ikeda~.class.sources := Code_source/Compiled/ikeda~.c
impseq~.class.sources := Code_source/Compiled/impseq~.c
trunc~.class.sources := Code_source/Compiled/trunc~.c
lastvalue~.class.sources := Code_source/Compiled/lastvalue~.c
latoocarfian~.class.sources := Code_source/Compiled/latoocarfian~.c
lorenz~.class.sources := Code_source/Compiled/lorenz~.c
lincong~.class.sources := Code_source/Compiled/lincong~.c
lin2db~.class.sources := Code_source/Compiled/lin2db~.c
logistic~.class.sources := Code_source/Compiled/logistic~.c
loop.class.sources := Code_source/Compiled/loop.c
lop2~.class.sources := Code_source/Compiled/lop2~.c
lowpass~.class.sources := Code_source/Compiled/lowpass~.c
lowshelf~.class.sources := Code_source/Compiled/lowshelf~.c
mov.rms~.class.sources := Code_source/Compiled/mov.rms~.c
mtx~.class.sources := Code_source/Compiled/mtx~.c
match~.class.sources := Code_source/Compiled/match~.c
mov.avg~.class.sources := Code_source/Compiled/mov.avg~.c
median~.class.sources := Code_source/Compiled/median~.c
mix~.class.sources := Code_source/Compiled/mix~.c
nchs~.class.sources := Code_source/Compiled/nchs~.c
nyquist~.class.sources := Code_source/Compiled/nyquist~.c
op~.class.sources := Code_source/Compiled/op~.c
pol2car~.class.sources := Code_source/Compiled/pol2car~.c
power~.class.sources := Code_source/Compiled/power~.c
pan2~.class.sources := Code_source/Compiled/pan2~.c
pan4~.class.sources := Code_source/Compiled/pan4~.c
peak~.class.sources := Code_source/Compiled/peak~.c
pmosc~.class.sources := Code_source/Compiled/pmosc~.c
phaseseq~.class.sources := Code_source/Compiled/phaseseq~.c
pulsecount~.class.sources := Code_source/Compiled/pulsecount~.c
pimpmul~.class.sources := Code_source/Compiled/pimpmul~.c
pulsediv~.class.sources := Code_source/Compiled/pulsediv~.c
quad~.class.sources := Code_source/Compiled/quad~.c
quantizer~.class.sources := Code_source/Compiled/quantizer~.c
ramp~.class.sources := Code_source/Compiled/ramp~.c
range~.class.sources := Code_source/Compiled/range~.c
ratio2cents~.class.sources := Code_source/Compiled/ratio2cents~.c
rescale~.class.sources := Code_source/Compiled/rescale~.c
rint~.class.sources := Code_source/Compiled/rint~.c
resonant~.class.sources := Code_source/Compiled/resonant~.c
resonant2~.class.sources := Code_source/Compiled/resonant2~.c
rms~.class.sources := Code_source/Compiled/rms~.c
rotate~.class.sources := Code_source/Compiled/rotate~.c
sh~.class.sources := Code_source/Compiled/sh~.c
schmitt~.class.sources := Code_source/Compiled/schmitt~.c
lag~.class.sources := Code_source/Compiled/lag~.c
lag2~.class.sources := Code_source/Compiled/lag2~.c
sin~.class.sources := Code_source/Compiled/sin~.c
sig2float~.class.sources := Code_source/Compiled/sig2float~.c
slew~.class.sources := Code_source/Compiled/slew~.c
slew2~.class.sources := Code_source/Compiled/slew2~.c
s2f~.class.sources := Code_source/extra_source/Aliases/s2f~.c
sequencer~.class.sources := Code_source/Compiled/sequencer~.c
sr~.class.sources := Code_source/Compiled/sr~.c
status~.class.sources := Code_source/Compiled/status~.c
standard~.class.sources := Code_source/Compiled/standard~.c
spread~.class.sources := Code_source/Compiled/spread~.c
susloop~.class.sources := Code_source/Compiled/susloop~.c
svfilter~.class.sources := Code_source/Compiled/svfilter~.c
trig.delay~.class.sources := Code_source/Compiled/trig.delay~.c
trig.delay2~.class.sources := Code_source/Compiled/trig.delay2~.c
timed.gate~.class.sources := Code_source/Compiled/timed.gate~.c
toggleff~.class.sources := Code_source/Compiled/toggleff~.c
trighold~.class.sources := Code_source/Compiled/trighold~.c
vu~.class.sources := Code_source/Compiled/vu~.c
xfade~.class.sources := Code_source/Compiled/xfade~.c
xgate~.class.sources := Code_source/Compiled/xgate~.c
xgate2~.class.sources := Code_source/Compiled/xgate2~.c
xmod~.class.sources := Code_source/Compiled/xmod~.c
xmod2~.class.sources := Code_source/Compiled/xmod2~.c
xselect~.class.sources := Code_source/Compiled/xselect~.c
xselect2~.class.sources := Code_source/Compiled/xselect2~.c
wrap2.class.sources := Code_source/Compiled/wrap2.c
wrap2~.class.sources := Code_source/Compiled/wrap2~.c
zerocross~.class.sources := Code_source/Compiled/zerocross~.c

aubio := $(wildcard Code_source/shared/aubio/src/*/*.c) $(wildcard Code_source/shared/aubio/src/*.c)
    beat~.class.sources := Code_source/Compiled/beat~.c $(aubio)

magic := Code_source/shared/magic.c
    sine~.class.sources := Code_source/Compiled/sine~.c $(magic)
    cosine~.class.sources := Code_source/Compiled/cosine~.c $(magic)
    fbsine~.class.sources := Code_source/Compiled/fbsine~.c $(magic)
    gaussian~.class.sources := Code_source/Compiled/gaussian~.c $(magic)
    imp~.class.sources := Code_source/extra_source/Aliases/imp~.c $(magic)
    impulse~.class.sources := Code_source/Compiled/impulse~.c $(magic)
    imp2~.class.sources := Code_source/extra_source/Aliases/imp2~.c $(magic)
    impulse2~.class.sources := Code_source/Compiled/impulse2~.c $(magic)
    parabolic~.class.sources := Code_source/Compiled/parabolic~.c $(magic)
    pulse~.class.sources := Code_source/Compiled/pulse~.c $(magic)
    saw~.class.sources := Code_source/Compiled/saw~.c $(magic)
    saw2~.class.sources := Code_source/Compiled/saw2~.c $(magic)
    square~.class.sources := Code_source/Compiled/square~.c $(magic)
    tri~.class.sources := Code_source/Compiled/tri~.c $(magic)
    vsaw~.class.sources := Code_source/Compiled/vsaw~.c $(magic)
    pimp~.class.sources := Code_source/Compiled/pimp~.c $(magic)
    numbox~.class.sources := Code_source/Compiled/numbox~.c $(magic)

buf := Code_source/shared/buffer.c
    shaper~.class.sources = Code_source/Compiled/shaper~.c $(buf)
    tabreader.class.sources = Code_source/Compiled/tabreader.c $(buf)
    tabreader~.class.sources = Code_source/Compiled/tabreader~.c $(buf)
    function.class.sources := Code_source/Compiled/function.c $(buf)
    function~.class.sources := Code_source/Compiled/function~.c $(buf)
    tabwriter~.class.sources = Code_source/Compiled/tabwriter~.c $(buf)
    del~.class.sources := Code_source/Compiled/del~.c $(buf)
    fbdelay~.class.sources := Code_source/Compiled/fbdelay~.c $(buf)
    ffdelay~.class.sources := Code_source/Compiled/ffdelay~.c $(buf)
    filterdelay~.class.sources := Code_source/Compiled/filterdelay~.c $(buf)

bufmagic := \
Code_source/shared/magic.c \
Code_source/shared/buffer.c
    wavetable~.class.sources = Code_source/Compiled/wavetable~.c $(bufmagic)
    wt~.class.sources = Code_source/extra_source/Aliases/wt~.c $(bufmagic)
    tabplayer~.class.sources = Code_source/Compiled/tabplayer~.c $(bufmagic)


randbuf := \
Code_source/shared/random.c \
Code_source/shared/buffer.c
    gendyn~.class.sources := Code_source/Compiled/gendyn~.c $(randbuf)

randmagic := \
Code_source/shared/magic.c \
Code_source/shared/random.c
    brown~.class.sources := Code_source/Compiled/brown~.c $(randmagic)

rand := Code_source/shared/random.c
    white~.class.sources := Code_source/Compiled/white~.c $(rand)
    pink~.class.sources := Code_source/Compiled/pink~.c $(rand)
    gray~.class.sources := Code_source/Compiled/gray~.c $(rand)
    pluck~.class.sources := Code_source/Compiled/pluck~.c $(rand)
    rand.u.class.sources := Code_source/Compiled/rand.u.c $(rand)
    rand.hist.class.sources := Code_source/Compiled/rand.hist.c $(rand)
    rand.i.class.sources := Code_source/Compiled/rand.i.c $(rand)
    rand.i~.class.sources := Code_source/Compiled/rand.i~.c $(rand)
    rand.f.class.sources := Code_source/Compiled/rand.f.c $(rand)
    rand.f~.class.sources := Code_source/Compiled/rand.f~.c $(rand)
    randpulse~.class.sources := Code_source/Compiled/randpulse~.c $(rand)
    randpulse2~.class.sources := Code_source/Compiled/randpulse2~.c $(rand)
    lfnoise~.class.sources := Code_source/Compiled/lfnoise~.c $(rand)
    rampnoise~.class.sources := Code_source/Compiled/rampnoise~.c $(rand)
    stepnoise~.class.sources := Code_source/Compiled/stepnoise~.c $(rand)
    dust~.class.sources := Code_source/Compiled/dust~.c $(rand)
    dust2~.class.sources := Code_source/Compiled/dust2~.c $(rand)
    chance.class.sources := Code_source/Compiled/chance.c $(rand)
    chance~.class.sources := Code_source/Compiled/chance~.c $(rand)
    tempo~.class.sources := Code_source/Compiled/tempo~.c $(rand)

midi := \
    Code_source/shared/mifi.c \
    Code_source/shared/elsefile.c
    midi.class.sources := Code_source/Compiled/midi.c $(midi)
    
file := Code_source/shared/elsefile.c
    rec.class.sources := Code_source/Compiled/rec.c $(file)

smagic := Code_source/shared/magic.c
    oscope~.class.sources := Code_source/Compiled/oscope~.c $(smagic)
    
utf := Code_source/shared/s_utf8.c
	note.class.sources := Code_source/Compiled/note.c $(utf)
    
define forWindows
  ldlibs += -lws2_32 
endef

#########################################################################

# extra files

extrafiles = \
$(wildcard Code_source/Abstractions/abs_objects/*.pd) \
$(wildcard Code_source/Abstractions/components/*.pd) \
$(wildcard Code_source/extra_source/*.tcl) \
$(wildcard Documentation/Help-files/*.pd) \
$(wildcard Documentation/extra_files/*.*) \
$(wildcard *.txt) \
README.pdf

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

#########################################################################

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# sfont subtargets. These let you build, install, and clean sfont~ without
# going into the sfont~ subdir. It also makes sure that sfont~ gets installed
# into the else (rather than its own sfont~) directory, so that the help patch
# will work as intended. Note that to make that work, any target installation
# directory (PDLIBDIR, objectsdir) should be specified as an absolute path.
# E.g.: make install sfont-install objectsdir=/usr/lib/pd/extra

sfont:
	$(MAKE) -C sfont~

sfont-install:
	$(MAKE) -C sfont~ install installpath="$(DESTDIR)$(PDLIBDIR)/else"

sfont-clean:
	$(MAKE) -C sfont~ clean

# Same for plaits.
# E.g.: make install plaits-install objectsdir=/usr/lib/pd/extra

plaits:
	$(MAKE) -C plaits~ $(plaitsflags)

plaits-install:
	$(MAKE) -C plaits~ install installpath="$(DESTDIR)$(PDLIBDIR)/else" $(plaitsflags)

plaits-clean:
	$(MAKE) -C plaits~ clean $(plaitsflags)
    
sfizz:
	$(MAKE) -C sfz~ system=$(system) 

sfizz-install:
	$(MAKE) -C sfz~ install system=$(system) exten=$(extension) installpath="$(abspath $(PDLIBDIR))/else"
    
sfizz-clean:
	$(MAKE) -C sfz~ clean

install: installplus

installplus:
	for v in $(extrafiles); do $(INSTALL_DATA) "$$v" "$(installpath)"; done
