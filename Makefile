############
# Makefile #
############

# library name

lib.name = else

aubioflags = -Ishared/aubio/src

define forDarwin
# old pdlibbuilder in plaits~ gets the target architecture(s) wrong
plaitsflags = arch="$(target.arch)"
endef

cflags = -Ishared -DHAVE_STRUCT_TIMESPEC $(aubioflags)

uname := $(shell uname -s)

#########################################################################
# Sources: ##############################################################
#########################################################################

# Lib:
else.class.sources := Code_source/Source/else.c

# GUI:
knob.class.sources := Code_source/Source/knob.c
button.class.sources := Code_source/Source/button.c
pic.class.sources := Code_source/Source/pic.c
keyboard.class.sources := Code_source/Source/keyboard.c
pad.class.sources := Code_source/Source/pad.c
openfile.class.sources := Code_source/Source/openfile.c
colors.class.sources := Code_source/Source/colors.c

# control:
args.class.sources := Code_source/Source/args.c
ctl.in.class.sources := Code_source/Source/ctl.in.c
ctl.out.class.sources := Code_source/Source/ctl.out.c
ceil.class.sources := Code_source/Source/ceil.c
suspedal.class.sources := Code_source/Source/suspedal.c
voices.class.sources := Code_source/Source/voices.c
buffer.class.sources := Code_source/Source/buffer_obj.c
bicoeff.class.sources := Code_source/Source/bicoeff.c
bicoeff2.class.sources := Code_source/Source/bicoeff2.c
click.class.sources := Code_source/Source/click.c
canvas.active.class.sources := Code_source/Source/canvas.active.c
canvas.bounds.class.sources := Code_source/Source/canvas.bounds.c
canvas.edit.class.sources := Code_source/Source/canvas.edit.c
canvas.file.class.sources := Code_source/Source/canvas.file.c
canvas.gop.class.sources := Code_source/Source/canvas.gop.c
canvas.mouse.class.sources := Code_source/Source/canvas.mouse.c
canvas.name.class.sources := Code_source/Source/canvas.name.c
canvas.pos.class.sources := Code_source/Source/canvas.pos.c
canvas.setname.class.sources := Code_source/Source/canvas.setname.c
canvas.vis.class.sources := Code_source/Source/canvas.vis.c
canvas.zoom.class.sources := Code_source/Source/canvas.zoom.c
properties.class.sources := Code_source/Source/properties.c
break.class.sources := Code_source/Source/break.c
cents2ratio.class.sources := Code_source/Source/cents2ratio.c
changed.class.sources := Code_source/Source/changed.c
gcd.class.sources := Code_source/Source/gcd.c
dir.class.sources := Code_source/Source/dir.c
datetime.class.sources := Code_source/Source/datetime.c
default.class.sources := Code_source/Source/default.c
dollsym.class.sources := Code_source/Source/dollsym.c
floor.class.sources := Code_source/Source/floor.c
fold.class.sources := Code_source/Source/fold.c
hot.class.sources := Code_source/Source/hot.c
hz2rad.class.sources := Code_source/Source/hz2rad.c
initmess.class.sources := Code_source/Source/initmess.c
keycode.class.sources := Code_source/Source/keycode.c
lb.class.sources := Code_source/Aliases/lb.c
limit.class.sources := Code_source/Source/limit.c
loadbanger.class.sources := Code_source/Source/loadbanger.c
merge.class.sources := Code_source/Source/merge.c
fontsize.class.sources := Code_source/Source/fontsize.c
format.class.sources := Code_source/Source/format.c
message.class.sources := Code_source/Source/message.c
messbox.class.sources := Code_source/Source/messbox.c
metronome.class.sources := Code_source/Source/metronome.c
mouse.class.sources := Code_source/Source/mouse.c
noteinfo.class.sources := Code_source/Source/noteinfo.c
note.in.class.sources := Code_source/Source/note.in.c
note.out.class.sources := Code_source/Source/note.out.c
osc.route.class.sources := Code_source/Source/osc.route.c
osc.parse.class.sources := Code_source/Source/osc.parse.c
osc.format.class.sources := Code_source/Source/osc.format.c
factor.class.sources := Code_source/Source/factor.c
float2bits.class.sources := Code_source/Source/float2bits.c
panic.class.sources := Code_source/Source/panic.c
pgm.in.class.sources := Code_source/Source/pgm.in.c
pgm.out.class.sources := Code_source/Source/pgm.out.c
pipe2.class.sources := Code_source/Source/pipe2.c
bend.in.class.sources := Code_source/Source/bend.in.c
bend.out.class.sources := Code_source/Source/bend.out.c
pack2.class.sources := Code_source/Source/pack2.c
quantizer.class.sources := Code_source/Source/quantizer.c
rad2hz.class.sources := Code_source/Source/rad2hz.c
ratio2cents.class.sources := Code_source/Source/ratio2cents.c
rescale.class.sources := Code_source/Source/rescale.c
oldrescale.class.sources := Code_source/Source/oldrescale.c
rint.class.sources := Code_source/Source/rint.c
router.class.sources := Code_source/Source/router.c
route2.class.sources := Code_source/Source/route2.c
routeall.class.sources := Code_source/Source/routeall.c
routetype.class.sources := Code_source/Source/routetype.c
receiver.class.sources := Code_source/Source/receiver.c
retrieve.class.sources := Code_source/Source/retrieve.c
selector.class.sources := Code_source/Source/selector.c
separate.class.sources := Code_source/Source/separate.c
symbol2any.class.sources := Code_source/Source/symbol2any.c
slice.class.sources := Code_source/Source/slice.c
sort.class.sources := Code_source/Source/sort.c
spread.class.sources := Code_source/Source/spread.c
touch.in.class.sources := Code_source/Source/touch.in.c
touch.out.class.sources := Code_source/Source/touch.out.c
trunc.class.sources := Code_source/Source/trunc.c
unmerge.class.sources := Code_source/Source/unmerge.c

# signal:
above~.class.sources := Code_source/Source/above~.c
add~.class.sources := Code_source/Source/add~.c
allpass.2nd~.class.sources := Code_source/Source/allpass.2nd~.c
allpass.rev~.class.sources := Code_source/Source/allpass.rev~.c
bitnormal~.class.sources := Code_source/Source/bitnormal~.c
comb.rev~.class.sources := Code_source/Source/comb.rev~.c
comb.filt~.class.sources := Code_source/Source/comb.filt~.c
adsr~.class.sources := Code_source/Source/adsr~.c
asr~.class.sources := Code_source/Source/asr~.c
autofade~.class.sources := Code_source/Source/autofade~.c
autofade2~.class.sources := Code_source/Source/autofade2~.c
balance~.class.sources := Code_source/Source/balance~.c
bandpass~.class.sources := Code_source/Source/bandpass~.c
bandstop~.class.sources := Code_source/Source/bandstop~.c
bl.imp~.class.sources := Code_source/Source/bl.imp~.c
bl.imp2~.class.sources := Code_source/Source/bl.imp2~.c
bl.saw~.class.sources := Code_source/Source/bl.saw~.c
bl.saw2~.class.sources := Code_source/Source/bl.saw2~.c
bl.square~.class.sources := Code_source/Source/bl.square~.c
bl.tri~.class.sources := Code_source/Source/bl.tri~.c
bl.vsaw~.class.sources := Code_source/Source/bl.vsaw~.c
blocksize~.class.sources := Code_source/Source/blocksize~.c
biquads~.class.sources := Code_source/Source/biquads~.c
car2pol~.class.sources := Code_source/Source/car2pol~.c
ceil~.class.sources := Code_source/Source/ceil~.c
cents2ratio~.class.sources := Code_source/Source/cents2ratio~.c
changed~.class.sources := Code_source/Source/changed~.c
changed2~.class.sources := Code_source/Source/changed2~.c
cmul~.class.sources := Code_source/Source/cmul~.c
crackle~.class.sources := Code_source/Source/crackle~.c
crossover~.class.sources := Code_source/Source/crossover~.c
cusp~.class.sources := Code_source/Source/cusp~.c
db2lin~.class.sources := Code_source/Source/db2lin~.c
decay~.class.sources := Code_source/Source/decay~.c
decay2~.class.sources := Code_source/Source/decay2~.c
downsample~.class.sources := Code_source/Source/downsample~.c
drive~.class.sources := Code_source/Source/drive~.c
detect~.class.sources := Code_source/Source/detect~.c
envgen~.class.sources := Code_source/Source/envgen~.c
eq~.class.sources := Code_source/Source/eq~.c
fader~.class.sources := Code_source/Source/fader~.c
fbsine2~.class.sources := Code_source/Source/fbsine2~.c
float2sig~.class.sources := Code_source/Source/float2sig~.c
f2s~.class.sources := Code_source/Aliases/f2s~.c
fdn.rev~.class.sources := Code_source/Source/fdn.rev~.c
floor~.class.sources := Code_source/Source/floor~.c
fold~.class.sources := Code_source/Source/fold~.c
freq.shift~.class.sources := Code_source/Source/freq.shift~.c
gbman~.class.sources := Code_source/Source/gbman~.c
gate2imp~.class.sources := Code_source/Source/gate2imp~.c
giga.rev~.class.sources := Code_source/Source/giga.rev~.c
glide~.class.sources := Code_source/Source/glide~.c
glide2~.class.sources := Code_source/Source/glide2~.c
henon~.class.sources := Code_source/Source/henon~.c
highpass~.class.sources := Code_source/Source/highpass~.c
highshelf~.class.sources := Code_source/Source/highshelf~.c
ikeda~.class.sources := Code_source/Source/ikeda~.c
impseq~.class.sources := Code_source/Source/impseq~.c
trunc~.class.sources := Code_source/Source/trunc~.c
lastvalue~.class.sources := Code_source/Source/lastvalue~.c
latoocarfian~.class.sources := Code_source/Source/latoocarfian~.c
lorenz~.class.sources := Code_source/Source/lorenz~.c
lincong~.class.sources := Code_source/Source/lincong~.c
lin2db~.class.sources := Code_source/Source/lin2db~.c
logistic~.class.sources := Code_source/Source/logistic~.c
loop.class.sources := Code_source/Source/loop.c
lop2~.class.sources := Code_source/Source/lop2~.c
lowpass~.class.sources := Code_source/Source/lowpass~.c
lowshelf~.class.sources := Code_source/Source/lowshelf~.c
mov.rms~.class.sources := Code_source/Source/mov.rms~.c
mtx~.class.sources := Code_source/Source/mtx~.c
match~.class.sources := Code_source/Source/match~.c
mov.avg~.class.sources := Code_source/Source/mov.avg~.c
median~.class.sources := Code_source/Source/median~.c
mix~.class.sources := Code_source/Source/mix~.c
nchs~.class.sources := Code_source/Source/nchs~.c
nyquist~.class.sources := Code_source/Source/nyquist~.c
op~.class.sources := Code_source/Source/op~.c
pol2car~.class.sources := Code_source/Source/pol2car~.c
power~.class.sources := Code_source/Source/power~.c
pan2~.class.sources := Code_source/Source/pan2~.c
pan4~.class.sources := Code_source/Source/pan4~.c
peak~.class.sources := Code_source/Source/peak~.c
pmosc~.class.sources := Code_source/Source/pmosc~.c
phaseseq~.class.sources := Code_source/Source/phaseseq~.c
pulsecount~.class.sources := Code_source/Source/pulsecount~.c
pimpmul~.class.sources := Code_source/Source/pimpmul~.c
pulsediv~.class.sources := Code_source/Source/pulsediv~.c
quad~.class.sources := Code_source/Source/quad~.c
quantizer~.class.sources := Code_source/Source/quantizer~.c
ramp~.class.sources := Code_source/Source/ramp~.c
range~.class.sources := Code_source/Source/range~.c
ratio2cents~.class.sources := Code_source/Source/ratio2cents~.c
rescale~.class.sources := Code_source/Source/rescale~.c
rint~.class.sources := Code_source/Source/rint~.c
resonant~.class.sources := Code_source/Source/resonant~.c
resonant2~.class.sources := Code_source/Source/resonant2~.c
rms~.class.sources := Code_source/Source/rms~.c
rotate~.class.sources := Code_source/Source/rotate~.c
sh~.class.sources := Code_source/Source/sh~.c
schmitt~.class.sources := Code_source/Source/schmitt~.c
lag~.class.sources := Code_source/Source/lag~.c
lag2~.class.sources := Code_source/Source/lag2~.c
sin~.class.sources := Code_source/Source/sin~.c
sig2float~.class.sources := Code_source/Source/sig2float~.c
slew~.class.sources := Code_source/Source/slew~.c
slew2~.class.sources := Code_source/Source/slew2~.c
s2f~.class.sources := Code_source/Aliases/s2f~.c
sequencer~.class.sources := Code_source/Source/sequencer~.c
sr~.class.sources := Code_source/Source/sr~.c
status~.class.sources := Code_source/Source/status~.c
standard~.class.sources := Code_source/Source/standard~.c
spread~.class.sources := Code_source/Source/spread~.c
susloop~.class.sources := Code_source/Source/susloop~.c
svfilter~.class.sources := Code_source/Source/svfilter~.c
trig.delay~.class.sources := Code_source/Source/trig.delay~.c
trig.delay2~.class.sources := Code_source/Source/trig.delay2~.c
timed.gate~.class.sources := Code_source/Source/timed.gate~.c
toggleff~.class.sources := Code_source/Source/toggleff~.c
trighold~.class.sources := Code_source/Source/trighold~.c
vu~.class.sources := Code_source/Source/vu~.c
xfade~.class.sources := Code_source/Source/xfade~.c
xgate~.class.sources := Code_source/Source/xgate~.c
xgate2~.class.sources := Code_source/Source/xgate2~.c
xmod~.class.sources := Code_source/Source/xmod~.c
xmod2~.class.sources := Code_source/Source/xmod2~.c
xselect~.class.sources := Code_source/Source/xselect~.c
xselect2~.class.sources := Code_source/Source/xselect2~.c
wrap2.class.sources := Code_source/Source/wrap2.c
wrap2~.class.sources := Code_source/Source/wrap2~.c
zerocross~.class.sources := Code_source/Source/zerocross~.c

aubio := $(wildcard shared/aubio/src/*/*.c) $(wildcard shared/aubio/src/*.c)
    beat~.class.sources := Code_source/Source/beat~.c $(aubio)

magic := shared/magic.c
    sine~.class.sources := Code_source/Source/sine~.c $(magic)
    cosine~.class.sources := Code_source/Source/cosine~.c $(magic)
    fbsine~.class.sources := Code_source/Source/fbsine~.c $(magic)
    gaussian~.class.sources := Code_source/Source/gaussian~.c $(magic)
    imp~.class.sources := Code_source/Aliases/imp~.c $(magic)
    impulse~.class.sources := Code_source/Source/impulse~.c $(magic)
    imp2~.class.sources := Code_source/Aliases/imp2~.c $(magic)
    impulse2~.class.sources := Code_source/Source/impulse2~.c $(magic)
    parabolic~.class.sources := Code_source/Source/parabolic~.c $(magic)
    pulse~.class.sources := Code_source/Source/pulse~.c $(magic)
    saw~.class.sources := Code_source/Source/saw~.c $(magic)
    saw2~.class.sources := Code_source/Source/saw2~.c $(magic)
    square~.class.sources := Code_source/Source/square~.c $(magic)
    tri~.class.sources := Code_source/Source/tri~.c $(magic)
    vsaw~.class.sources := Code_source/Source/vsaw~.c $(magic)
    pimp~.class.sources := Code_source/Source/pimp~.c $(magic)
    numbox~.class.sources := Code_source/Source/numbox~.c $(magic)

buf := shared/buffer.c
    shaper~.class.sources = Code_source/Source/shaper~.c $(buf)
    tabreader.class.sources = Code_source/Source/tabreader.c $(buf)
    tabreader~.class.sources = Code_source/Source/tabreader~.c $(buf)
    function.class.sources := Code_source/Source/function.c $(buf)
    function~.class.sources := Code_source/Source/function~.c $(buf)
    tabwriter~.class.sources = Code_source/Source/tabwriter~.c $(buf)
    del~.class.sources := Code_source/Source/del~.c $(buf)
    fbdelay~.class.sources := Code_source/Source/fbdelay~.c $(buf)
    ffdelay~.class.sources := Code_source/Source/ffdelay~.c $(buf)
    filterdelay~.class.sources := Code_source/Source/filterdelay~.c $(buf)

bufmagic := \
shared/magic.c \
shared/buffer.c
    wavetable~.class.sources = Code_source/Source/wavetable~.c $(bufmagic)
    wt~.class.sources = Code_source/Aliases/wt~.c $(bufmagic)
    tabplayer~.class.sources = Code_source/Source/tabplayer~.c $(bufmagic)


randbuf := \
shared/random.c \
shared/buffer.c
    gendyn~.class.sources := Code_source/Source/gendyn~.c $(randbuf)

randmagic := \
shared/magic.c \
shared/random.c
    brown~.class.sources := Code_source/Source/brown~.c $(randmagic)

rand := shared/random.c
    white~.class.sources := Code_source/Source/white~.c $(rand)
    pink~.class.sources := Code_source/Source/pink~.c $(rand)
    gray~.class.sources := Code_source/Source/gray~.c $(rand)
    pluck~.class.sources := Code_source/Source/pluck~.c $(rand)
    rand.u.class.sources := Code_source/Source/rand.u.c $(rand)
    rand.hist.class.sources := Code_source/Source/rand.hist.c $(rand)
    rand.i.class.sources := Code_source/Source/rand.i.c $(rand)
    rand.i~.class.sources := Code_source/Source/rand.i~.c $(rand)
    rand.f.class.sources := Code_source/Source/rand.f.c $(rand)
    rand.f~.class.sources := Code_source/Source/rand.f~.c $(rand)
    randpulse~.class.sources := Code_source/Source/randpulse~.c $(rand)
    randpulse2~.class.sources := Code_source/Source/randpulse2~.c $(rand)
    lfnoise~.class.sources := Code_source/Source/lfnoise~.c $(rand)
    rampnoise~.class.sources := Code_source/Source/rampnoise~.c $(rand)
    stepnoise~.class.sources := Code_source/Source/stepnoise~.c $(rand)
    dust~.class.sources := Code_source/Source/dust~.c $(rand)
    dust2~.class.sources := Code_source/Source/dust2~.c $(rand)
    chance.class.sources := Code_source/Source/chance.c $(rand)
    chance~.class.sources := Code_source/Source/chance~.c $(rand)
    tempo~.class.sources := Code_source/Source/tempo~.c $(rand)

midi := \
    shared/mifi.c \
    shared/elsefile.c
    midi.class.sources := Code_source/Source/midi.c $(midi)
    
file := shared/elsefile.c
    rec.class.sources := Code_source/Source/rec.c $(file)

smagic := shared/magic.c
    oscope~.class.sources := Code_source/Source/oscope~.c $(smagic)
    
utf := shared/s_utf8.c
	note.class.sources := Code_source/Source/note.c $(utf)
    
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
