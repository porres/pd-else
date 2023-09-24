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
else.class.sources := Code_source/Compiled/control/else.c

# GUI:
knob.class.sources := Code_source/Compiled/control/knob.c
button.class.sources := Code_source/Compiled/control/button.c
pic.class.sources := Code_source/Compiled/control/pic.c
keyboard.class.sources := Code_source/Compiled/control/keyboard.c
pad.class.sources := Code_source/Compiled/control/pad.c
openfile.class.sources := Code_source/Compiled/control/openfile.c
colors.class.sources := Code_source/Compiled/control/colors.c

# control:
args.class.sources := Code_source/Compiled/control/args.c
ctl.in.class.sources := Code_source/Compiled/control/ctl.in.c
ctl.out.class.sources := Code_source/Compiled/control/ctl.out.c
ceil.class.sources := Code_source/Compiled/control/ceil.c
suspedal.class.sources := Code_source/Compiled/control/suspedal.c
voices.class.sources := Code_source/Compiled/control/voices.c
buffer.class.sources := Code_source/Compiled/control/buffer_obj.c
bicoeff.class.sources := Code_source/Compiled/control/bicoeff.c
bicoeff2.class.sources := Code_source/Compiled/control/bicoeff2.c
click.class.sources := Code_source/Compiled/control/click.c
canvas.active.class.sources := Code_source/Compiled/control/canvas.active.c
canvas.bounds.class.sources := Code_source/Compiled/control/canvas.bounds.c
canvas.edit.class.sources := Code_source/Compiled/control/canvas.edit.c
canvas.file.class.sources := Code_source/Compiled/control/canvas.file.c
canvas.gop.class.sources := Code_source/Compiled/control/canvas.gop.c
canvas.mouse.class.sources := Code_source/Compiled/control/canvas.mouse.c
canvas.name.class.sources := Code_source/Compiled/control/canvas.name.c
canvas.pos.class.sources := Code_source/Compiled/control/canvas.pos.c
canvas.setname.class.sources := Code_source/Compiled/control/canvas.setname.c
canvas.vis.class.sources := Code_source/Compiled/control/canvas.vis.c
canvas.zoom.class.sources := Code_source/Compiled/control/canvas.zoom.c
properties.class.sources := Code_source/Compiled/control/properties.c
break.class.sources := Code_source/Compiled/control/break.c
cents2ratio.class.sources := Code_source/Compiled/control/cents2ratio.c
changed.class.sources := Code_source/Compiled/control/changed.c
gcd.class.sources := Code_source/Compiled/control/gcd.c
dir.class.sources := Code_source/Compiled/control/dir.c
datetime.class.sources := Code_source/Compiled/control/datetime.c
default.class.sources := Code_source/Compiled/control/default.c
dollsym.class.sources := Code_source/Compiled/control/dollsym.c
floor.class.sources := Code_source/Compiled/control/floor.c
fold.class.sources := Code_source/Compiled/control/fold.c
hot.class.sources := Code_source/Compiled/control/hot.c
hz2rad.class.sources := Code_source/Compiled/control/hz2rad.c
initmess.class.sources := Code_source/Compiled/control/initmess.c
keycode.class.sources := Code_source/Compiled/control/keycode.c
lb.class.sources := Code_source/Compiled/extra_source/Aliases/lb.c
limit.class.sources := Code_source/Compiled/control/limit.c
loadbanger.class.sources := Code_source/Compiled/control/loadbanger.c
merge.class.sources := Code_source/Compiled/control/merge.c
fontsize.class.sources := Code_source/Compiled/control/fontsize.c
format.class.sources := Code_source/Compiled/control/format.c
message.class.sources := Code_source/Compiled/control/message.c
messbox.class.sources := Code_source/Compiled/control/messbox.c
metronome.class.sources := Code_source/Compiled/control/metronome.c
mouse.class.sources := Code_source/Compiled/control/mouse.c
noteinfo.class.sources := Code_source/Compiled/control/noteinfo.c
note.in.class.sources := Code_source/Compiled/control/note.in.c
note.out.class.sources := Code_source/Compiled/control/note.out.c
order.class.sources := Code_source/Compiled/control/order.c
osc.route.class.sources := Code_source/Compiled/control/osc.route.c
osc.parse.class.sources := Code_source/Compiled/control/osc.parse.c
osc.format.class.sources := Code_source/Compiled/control/osc.format.c
factor.class.sources := Code_source/Compiled/control/factor.c
float2bits.class.sources := Code_source/Compiled/control/float2bits.c
panic.class.sources := Code_source/Compiled/control/panic.c
pgm.in.class.sources := Code_source/Compiled/control/pgm.in.c
pgm.out.class.sources := Code_source/Compiled/control/pgm.out.c
pipe2.class.sources := Code_source/Compiled/control/pipe2.c
bend.in.class.sources := Code_source/Compiled/control/bend.in.c
bend.out.class.sources := Code_source/Compiled/control/bend.out.c
pack2.class.sources := Code_source/Compiled/control/pack2.c
quantizer.class.sources := Code_source/Compiled/control/quantizer.c
rad2hz.class.sources := Code_source/Compiled/control/rad2hz.c
ratio2cents.class.sources := Code_source/Compiled/control/ratio2cents.c
rescale.class.sources := Code_source/Compiled/control/rescale.c
rint.class.sources := Code_source/Compiled/control/rint.c
router.class.sources := Code_source/Compiled/control/router.c
route2.class.sources := Code_source/Compiled/control/route2.c
routeall.class.sources := Code_source/Compiled/control/routeall.c
routetype.class.sources := Code_source/Compiled/control/routetype.c
receiver.class.sources := Code_source/Compiled/control/receiver.c
retrieve.class.sources := Code_source/Compiled/control/retrieve.c
selector.class.sources := Code_source/Compiled/control/selector.c
sender.class.sources := Code_source/Compiled/control/sender.c
separate.class.sources := Code_source/Compiled/control/separate.c
symbol2any.class.sources := Code_source/Compiled/control/symbol2any.c
slice.class.sources := Code_source/Compiled/control/slice.c
sort.class.sources := Code_source/Compiled/control/sort.c
spread.class.sources := Code_source/Compiled/control/spread.c
touch.in.class.sources := Code_source/Compiled/control/touch.in.c
touch.out.class.sources := Code_source/Compiled/control/touch.out.c
ptouch.in.class.sources := Code_source/Compiled/control/ptouch.in.c
ptouch.out.class.sources := Code_source/Compiled/control/ptouch.out.c
trunc.class.sources := Code_source/Compiled/control/trunc.c
unmerge.class.sources := Code_source/Compiled/control/unmerge.c

# signal:
above~.class.sources := Code_source/Compiled/audio/above~.c
add~.class.sources := Code_source/Compiled/audio/add~.c
allpass.2nd~.class.sources := Code_source/Compiled/audio/allpass.2nd~.c
allpass.rev~.class.sources := Code_source/Compiled/audio/allpass.rev~.c
bitnormal~.class.sources := Code_source/Compiled/audio/bitnormal~.c
comb.rev~.class.sources := Code_source/Compiled/audio/comb.rev~.c
comb.filt~.class.sources := Code_source/Compiled/audio/comb.filt~.c
adsr~.class.sources := Code_source/Compiled/audio/adsr~.c
asr~.class.sources := Code_source/Compiled/audio/asr~.c
autofade~.class.sources := Code_source/Compiled/audio/autofade~.c
autofade2~.class.sources := Code_source/Compiled/audio/autofade2~.c
balance~.class.sources := Code_source/Compiled/audio/balance~.c
bandpass~.class.sources := Code_source/Compiled/audio/bandpass~.c
bandstop~.class.sources := Code_source/Compiled/audio/bandstop~.c
bl.imp~.class.sources := Code_source/Compiled/audio/bl.imp~.c
bl.imp2~.class.sources := Code_source/Compiled/audio/bl.imp2~.c
bl.saw~.class.sources := Code_source/Compiled/audio/bl.saw~.c
bl.saw2~.class.sources := Code_source/Compiled/audio/bl.saw2~.c
bl.square~.class.sources := Code_source/Compiled/audio/bl.square~.c
bl.tri~.class.sources := Code_source/Compiled/audio/bl.tri~.c
bl.vsaw~.class.sources := Code_source/Compiled/audio/bl.vsaw~.c
blocksize~.class.sources := Code_source/Compiled/audio/blocksize~.c
biquads~.class.sources := Code_source/Compiled/audio/biquads~.c
car2pol~.class.sources := Code_source/Compiled/audio/car2pol~.c
ceil~.class.sources := Code_source/Compiled/audio/ceil~.c
cents2ratio~.class.sources := Code_source/Compiled/audio/cents2ratio~.c
changed~.class.sources := Code_source/Compiled/audio/changed~.c
changed2~.class.sources := Code_source/Compiled/audio/changed2~.c
conv~.class.sources := Code_source/Compiled/audio/conv~.c
cmul~.class.sources := Code_source/Compiled/audio/cmul~.c
crackle~.class.sources := Code_source/Compiled/audio/crackle~.c
crossover~.class.sources := Code_source/Compiled/audio/crossover~.c
cusp~.class.sources := Code_source/Compiled/audio/cusp~.c
db2lin~.class.sources := Code_source/Compiled/audio/db2lin~.c
decay~.class.sources := Code_source/Compiled/audio/decay~.c
decay2~.class.sources := Code_source/Compiled/audio/decay2~.c
downsample~.class.sources := Code_source/Compiled/audio/downsample~.c
drive~.class.sources := Code_source/Compiled/audio/drive~.c
detect~.class.sources := Code_source/Compiled/audio/detect~.c
envgen~.class.sources := Code_source/Compiled/audio/envgen~.c
eq~.class.sources := Code_source/Compiled/audio/eq~.c
fader~.class.sources := Code_source/Compiled/audio/fader~.c
fbsine2~.class.sources := Code_source/Compiled/audio/fbsine2~.c
fdn.rev~.class.sources := Code_source/Compiled/audio/fdn.rev~.c
floor~.class.sources := Code_source/Compiled/audio/floor~.c
fold~.class.sources := Code_source/Compiled/audio/fold~.c
freq.shift~.class.sources := Code_source/Compiled/audio/freq.shift~.c
gbman~.class.sources := Code_source/Compiled/audio/gbman~.c
gate2imp~.class.sources := Code_source/Compiled/audio/gate2imp~.c
get~.class.sources := Code_source/Compiled/audio/get~.c
giga.rev~.class.sources := Code_source/Compiled/audio/giga.rev~.c
glide~.class.sources := Code_source/Compiled/audio/glide~.c
glide2~.class.sources := Code_source/Compiled/audio/glide2~.c
henon~.class.sources := Code_source/Compiled/audio/henon~.c
highpass~.class.sources := Code_source/Compiled/audio/highpass~.c
highshelf~.class.sources := Code_source/Compiled/audio/highshelf~.c
ikeda~.class.sources := Code_source/Compiled/audio/ikeda~.c
impseq~.class.sources := Code_source/Compiled/audio/impseq~.c
trunc~.class.sources := Code_source/Compiled/audio/trunc~.c
lastvalue~.class.sources := Code_source/Compiled/audio/lastvalue~.c
latoocarfian~.class.sources := Code_source/Compiled/audio/latoocarfian~.c
lorenz~.class.sources := Code_source/Compiled/audio/lorenz~.c
lincong~.class.sources := Code_source/Compiled/audio/lincong~.c
lin2db~.class.sources := Code_source/Compiled/audio/lin2db~.c
logistic~.class.sources := Code_source/Compiled/audio/logistic~.c
loop.class.sources := Code_source/Compiled/control/loop.c
lop2~.class.sources := Code_source/Compiled/audio/lop2~.c
lowpass~.class.sources := Code_source/Compiled/audio/lowpass~.c
lowshelf~.class.sources := Code_source/Compiled/audio/lowshelf~.c
mov.rms~.class.sources := Code_source/Compiled/audio/mov.rms~.c
mtx~.class.sources := Code_source/Compiled/audio/mtx~.c
match~.class.sources := Code_source/Compiled/audio/match~.c
mov.avg~.class.sources := Code_source/Compiled/audio/mov.avg~.c
median~.class.sources := Code_source/Compiled/audio/median~.c
merge~.class.sources := Code_source/Compiled/audio/merge~.c
nchs~.class.sources := Code_source/Compiled/audio/nchs~.c
nyquist~.class.sources := Code_source/Compiled/audio/nyquist~.c
op~.class.sources := Code_source/Compiled/audio/op~.c
pol2car~.class.sources := Code_source/Compiled/audio/pol2car~.c
power~.class.sources := Code_source/Compiled/audio/power~.c
pan2~.class.sources := Code_source/Compiled/audio/pan2~.c
pan4~.class.sources := Code_source/Compiled/audio/pan4~.c
peak~.class.sources := Code_source/Compiled/audio/peak~.c
pmosc~.class.sources := Code_source/Compiled/audio/pmosc~.c
phaseseq~.class.sources := Code_source/Compiled/audio/phaseseq~.c
pulsecount~.class.sources := Code_source/Compiled/audio/pulsecount~.c
pick~.class.sources := Code_source/Compiled/audio/pick~.c
pimpmul~.class.sources := Code_source/Compiled/audio/pimpmul~.c
pulsediv~.class.sources := Code_source/Compiled/audio/pulsediv~.c
quad~.class.sources := Code_source/Compiled/audio/quad~.c
quantizer~.class.sources := Code_source/Compiled/audio/quantizer~.c
ramp~.class.sources := Code_source/Compiled/audio/ramp~.c
range~.class.sources := Code_source/Compiled/audio/range~.c
ratio2cents~.class.sources := Code_source/Compiled/audio/ratio2cents~.c
rescale~.class.sources := Code_source/Compiled/audio/rescale~.c
rint~.class.sources := Code_source/Compiled/audio/rint~.c
repeat~.class.sources := Code_source/Compiled/audio/repeat~.c
resonant~.class.sources := Code_source/Compiled/audio/resonant~.c
resonant2~.class.sources := Code_source/Compiled/audio/resonant2~.c
rms~.class.sources := Code_source/Compiled/audio/rms~.c
rotate~.class.sources := Code_source/Compiled/audio/rotate~.c
rotate.mc~.class.sources := Code_source/Compiled/audio/rotate.mc~.c
sh~.class.sources := Code_source/Compiled/audio/sh~.c
schmitt~.class.sources := Code_source/Compiled/audio/schmitt~.c
slice~.class.sources := Code_source/Compiled/audio/slice~.c
lag~.class.sources := Code_source/Compiled/audio/lag~.c
lag2~.class.sources := Code_source/Compiled/audio/lag2~.c
sig2float~.class.sources := Code_source/Compiled/audio/sig2float~.c
slew~.class.sources := Code_source/Compiled/audio/slew~.c
slew2~.class.sources := Code_source/Compiled/audio/slew2~.c
s2f~.class.sources := Code_source/Compiled/extra_source/Aliases/s2f~.c
sequencer~.class.sources := Code_source/Compiled/audio/sequencer~.c
select~.class.sources := Code_source/Compiled/audio/select~.c
sr~.class.sources := Code_source/Compiled/audio/sr~.c
status~.class.sources := Code_source/Compiled/audio/status~.c
standard~.class.sources := Code_source/Compiled/audio/standard~.c
sum~.class.sources := Code_source/Compiled/audio/sum~.c
sigs~.class.sources := Code_source/Compiled/audio/sigs~.c
spread~.class.sources := Code_source/Compiled/audio/spread~.c
spread.mc~.class.sources := Code_source/Compiled/audio/spread.mc~.c
susloop~.class.sources := Code_source/Compiled/audio/susloop~.c
svfilter~.class.sources := Code_source/Compiled/audio/svfilter~.c
trig.delay~.class.sources := Code_source/Compiled/audio/trig.delay~.c
trig.delay2~.class.sources := Code_source/Compiled/audio/trig.delay2~.c
timed.gate~.class.sources := Code_source/Compiled/audio/timed.gate~.c
toggleff~.class.sources := Code_source/Compiled/audio/toggleff~.c
trighold~.class.sources := Code_source/Compiled/audio/trighold~.c
unmerge~.class.sources := Code_source/Compiled/audio/unmerge~.c
vu~.class.sources := Code_source/Compiled/audio/vu~.c
xfade~.class.sources := Code_source/Compiled/audio/xfade~.c
xfade.mc~.class.sources := Code_source/Compiled/audio/xfade.mc~.c
xgate~.class.sources := Code_source/Compiled/audio/xgate~.c
xgate.mc~.class.sources := Code_source/Compiled/audio/xgate.mc~.c
xgate2~.class.sources := Code_source/Compiled/audio/xgate2~.c
xmod~.class.sources := Code_source/Compiled/audio/xmod~.c
xmod2~.class.sources := Code_source/Compiled/audio/xmod2~.c
xselect~.class.sources := Code_source/Compiled/audio/xselect~.c
xselect.mc~.class.sources := Code_source/Compiled/audio/xselect.mc~.c
xselect2~.class.sources := Code_source/Compiled/audio/xselect2~.c
wrap2.class.sources := Code_source/Compiled/control/wrap2.c
wrap2~.class.sources := Code_source/Compiled/audio/wrap2~.c
zerocross~.class.sources := Code_source/Compiled/audio/zerocross~.c

magic := Code_source/shared/magic.c
    gaussian~.class.sources := Code_source/Compiled/audio/gaussian~.c $(magic)
    imp~.class.sources := Code_source/Compiled/extra_source/Aliases/imp~.c $(magic)
    impulse~.class.sources := Code_source/Compiled/audio/impulse~.c $(magic)
    imp2~.class.sources := Code_source/Compiled/extra_source/Aliases/imp2~.c $(magic)
    impulse2~.class.sources := Code_source/Compiled/audio/impulse2~.c $(magic)
    parabolic~.class.sources := Code_source/Compiled/audio/parabolic~.c $(magic)
    pulse~.class.sources := Code_source/Compiled/audio/pulse~.c $(magic)
    saw~.class.sources := Code_source/Compiled/audio/saw~.c $(magic)
    saw2~.class.sources := Code_source/Compiled/audio/saw2~.c $(magic)
    square~.class.sources := Code_source/Compiled/audio/square~.c $(magic)
    tri~.class.sources := Code_source/Compiled/audio/tri~.c $(magic)
    vsaw~.class.sources := Code_source/Compiled/audio/vsaw~.c $(magic)
    pimp~.class.sources := Code_source/Compiled/audio/pimp~.c $(magic)
    numbox~.class.sources := Code_source/Compiled/audio/numbox~.c $(magic)

buf := Code_source/shared/buffer.c
    sin~.class.sources := Code_source/Compiled/audio/sin~.c $(buf)
    op2~.class.sources := Code_source/Compiled/audio/op2~.c $(buf)
    op4~.class.sources := Code_source/Compiled/audio/op4~.c $(buf)
    op6~.class.sources := Code_source/Compiled/audio/op6~.c $(buf)
    shaper~.class.sources = Code_source/Compiled/audio/shaper~.c $(buf)
    tabreader.class.sources = Code_source/Compiled/control/tabreader.c $(buf)
    tabreader~.class.sources = Code_source/Compiled/audio/tabreader~.c $(buf)
    function.class.sources := Code_source/Compiled/control/function.c $(buf)
    function~.class.sources := Code_source/Compiled/audio/function~.c $(buf)
    tabwriter~.class.sources = Code_source/Compiled/audio/tabwriter~.c $(buf)
    del~.class.sources := Code_source/Compiled/audio/del~.c $(buf)
    fbdelay~.class.sources := Code_source/Compiled/audio/fbdelay~.c $(buf)
    ffdelay~.class.sources := Code_source/Compiled/audio/ffdelay~.c $(buf)
    filterdelay~.class.sources := Code_source/Compiled/audio/filterdelay~.c $(buf)

bufmagic := \
Code_source/shared/magic.c \
Code_source/shared/buffer.c
    cosine~.class.sources := Code_source/Compiled/audio/cosine~.c $(bufmagic)
    fbsine~.class.sources := Code_source/Compiled/audio/fbsine~.c $(bufmagic)
    sine~.class.sources := Code_source/Compiled/audio/sine~.c $(bufmagic)
    wavetable~.class.sources = Code_source/Compiled/audio/wavetable~.c $(bufmagic)
    wt~.class.sources = Code_source/Compiled/extra_source/Aliases/wt~.c $(bufmagic)
    tabplayer~.class.sources = Code_source/Compiled/audio/tabplayer~.c $(bufmagic)


randbuf := \
Code_source/shared/random.c \
Code_source/shared/buffer.c
    gendyn~.class.sources := Code_source/Compiled/audio/gendyn~.c $(randbuf)

randmagic := \
Code_source/shared/magic.c \
Code_source/shared/random.c
    brown~.class.sources := Code_source/Compiled/audio/brown~.c $(randmagic)

rand := Code_source/shared/random.c
    white~.class.sources := Code_source/Compiled/audio/white~.c $(rand)
    pink~.class.sources := Code_source/Compiled/audio/pink~.c $(rand)
    gray~.class.sources := Code_source/Compiled/audio/gray~.c $(rand)
    pluck~.class.sources := Code_source/Compiled/audio/pluck~.c $(rand)
    rand.u.class.sources := Code_source/Compiled/control/rand.u.c $(rand)
    rand.hist.class.sources := Code_source/Compiled/control/rand.hist.c $(rand)
    rand.i.class.sources := Code_source/Compiled/control/rand.i.c $(rand)
    rand.i~.class.sources := Code_source/Compiled/audio/rand.i~.c $(rand)
    rand.f.class.sources := Code_source/Compiled/control/rand.f.c $(rand)
    rand.f~.class.sources := Code_source/Compiled/audio/rand.f~.c $(rand)
    randpulse~.class.sources := Code_source/Compiled/audio/randpulse~.c $(rand)
    randpulse2~.class.sources := Code_source/Compiled/audio/randpulse2~.c $(rand)
    lfnoise~.class.sources := Code_source/Compiled/audio/lfnoise~.c $(rand)
    rampnoise~.class.sources := Code_source/Compiled/audio/rampnoise~.c $(rand)
    stepnoise~.class.sources := Code_source/Compiled/audio/stepnoise~.c $(rand)
    dust~.class.sources := Code_source/Compiled/audio/dust~.c $(rand)
    dust2~.class.sources := Code_source/Compiled/audio/dust2~.c $(rand)
    chance.class.sources := Code_source/Compiled/control/chance.c $(rand)
    chance~.class.sources := Code_source/Compiled/audio/chance~.c $(rand)
    tempo~.class.sources := Code_source/Compiled/audio/tempo~.c $(rand)

midi := \
    Code_source/shared/mifi.c \
    Code_source/shared/elsefile.c
    midi.class.sources := Code_source/Compiled/control/midi.c $(midi)
    
file := Code_source/shared/elsefile.c
    rec.class.sources := Code_source/Compiled/control/rec.c $(file)

smagic := Code_source/shared/magic.c
    oscope~.class.sources := Code_source/Compiled/audio/oscope~.c $(smagic)
    
utf := Code_source/shared/s_elseutf8.c
	note.class.sources := Code_source/Compiled/control/note.c $(utf)
    
define forWindows
  ldlibs += -lws2_32 
endef

#########################################################################

# extra files

extrafiles = \
$(wildcard Code_source/Abstractions/control/*.pd) \
$(wildcard Code_source/Abstractions/audio/*.pd) \
$(wildcard Code_source/Abstractions/extra_abs/*.pd) \
$(wildcard Code_source/Compiled/extra_source/*.tcl) \
$(wildcard Documentation/Help-files/*.pd) \
$(wildcard Documentation/extra_files/*.*) \
$(wildcard *.txt) \
Documentation/README.pdf

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
	$(MAKE) -C Code_source/Compiled/audio/sfont~

sfont-install:
	$(MAKE) -C Code_source/Compiled/audio/sfont~ install installpath="$(DESTDIR)$(PDLIBDIR)/else"

sfont-clean:
	$(MAKE) -C Code_source/Compiled/audio/sfont~ clean

# Same for plaits

plaits:
	$(MAKE) -C Code_source/Compiled/audio/plaits~ $(plaitsflags)

plaits-install:
	$(MAKE) -C Code_source/Compiled/audio/plaits~ install installpath="$(DESTDIR)$(PDLIBDIR)/else" $(plaitsflags)

plaits-clean:
	$(MAKE) -C Code_source/Compiled/audio/plaits~ clean $(plaitsflags)
    
# Same for sfz    

sfz:
	$(MAKE) -C Code_source/Compiled/audio/sfz~ system=$(system)

sfz-install:
	$(MAKE) -C Code_source/Compiled/audio/sfz~ install system=$(system) exten=$(extension) installpath="$(abspath $(PDLIBDIR))/else"
    
sfz-clean:
	$(MAKE) -C Code_source/Compiled/audio/sfz~ clean

.PHONY: circuit

circuit:
	cd Code_source/Compiled/audio/circuit~ && ./build.sh

circuit-clean:
	rm -rf Code_source/Compiled/audio/circuit~/build

install: installplus

installplus:
	for v in $(extrafiles); do $(INSTALL_DATA) "$$v" "$(installpath)"; done
