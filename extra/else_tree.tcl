{else
    {gui
        {mtx.tcl pic pad function colors openfile biplot slider2d circle keyboard range.hsl mix2~ mix4~ meter~ meter2~ meter4~ meter8~ drum.seq graph~ spectrograph~ setdsp~ messbox button out1~ out~ out4~ out8~ gain~ gain2~ display numbox~ zbiplot oscope~ multi.vsl bicoeff}}        
    {oscillators
        {sine~ cosine~ saw~ saw2~ square~ tri~ vsaw~ pulse~ impulse~ impulse2~ parabolic~ oscbank~ oscbank2~ gaussian~ pmosc~ wavetable~ bl.osc~ bl.blip~ bl.imp~ bl.imp2~ bl.saw~ bl.saw2~ bl.square~ bl.tri~ bl.vsaw~ bl.wavetable~}}
    {synths
        {grain.synth~ pluck~ sfont~ plaits~}}
    {noise\ chaos\ stochastic
        {gendyn~ white~ pink~ brown~ gray~ lfnoise~ rampnoise~ stepnoise~ randpulse~ randpulse2~ perlin~ fbsine~ fbsine2~ xmod~ xmod2~ crackle~ cusp~ gbmnan~ henon~ ikeda~ latoocarfian~ lorenz~ lincong~ logistic~ quad~ standard~}}
    {assorted
        {else chrono datetime}}
    {fft
        {hann~ bin.shift~}}  
    {tuning/scale\ tools 
        {scales autotune autotune2 makenote2 scala retune eqdiv scale2cents cents2scale cents2frac frac2cents frac2dec dec2frac freq2midi midi2freq note2pitch pitch2note note2dur}}
    {patch/subpatch\ management
        {args meter presets blocksize~ fontsize retrieve canvas.active canvas.bounds canvas.file canvas.name canvas.vis canvas.edit canvas.setname canvas.zoom click properties dollsym receiver loadbanger}}
    {list\ management
        {break order group combine scramble interpolate delete replace morph sort iterate insert reverse rotate sum slice stream merge unmerge amean gmean}}
    {message\ management
        {swap2 nmess format unite separate route2 any2symbol symbol2any changed hot limit initmess default message pack2 pick spread router routeall routetype selector stack store pipe2 sig2float~ float2sig~}}
    {file\ management
        {dir}}
    {math:\ functions
        {add add~ median avg mov.avg frac.add frac.mul lcm gcd count ceil ceil~ factor floor floor~ trunc trunc~ rint rint~ quantizer quantizer~ fold fold~ lastvalue mag mag~ sin~ wrap2 wrap2~ op op~ cmul~}}
    {math:\ conversion
        {hex2dec dec2hex bpm car2pol car2pol~ pol2car pol2car~ pz2coeff coeff2pz cents2ratio cents2ratio~ ratio2cents ratio2cents~ ms2samps ms2samps~ samps2ms samps2ms~ deg2grad rad2deg db2lin db2lin~ float2bits hz2rad rad2hz lin2db lin2db~ rescale rescale~}}
    {math:\ constant
        {sr~ nyquist~ pi e}}
    {math:\ logic
        {loop}}
    {fx:\ assorted
        {downsample~ conv~ chorus~ del~\ in del~\ out fbdelay~ ffdelay~ shaper~ crusher~ power~ drive~ flanger~ freq.shift~ pitch.shift~ ping.pong~ phaser~ rm~ tremolo~ vibrato~ vocoder~ morph~ freeze~ pvoc.freeze~}}
    {fx:\ filters
        {allpass.2nd~ allpass.filt~ bitnormal~ comb.filt~ lop2~ lop.bw~ hip.bw~ biquads~ bandpass~ bandstop~ crossover~ bpbank~ bicoeff2 brickwall~ eq~ highpass~ highshelf~ lowpass~ lowshelf~ mov.avg~ resonbank~ resonbank2~ resonant~ resonant2~ svfilter~}}
    {fx:\ reverberators
        {allpass.rev~ comb.rev~ mono.rev~ stereo.rev~ plate.rev~ giga.rev~ free.rev~ fdn.rev~}}
    {fx:\ dynamics
        {compress~ duck~ expand~ noisegate~ norm~}}
    {table/sampling/players/granulation
        {buffer tabgen tabreader tabreader~ tabwriter~ tabplayer~ sample~ batch.rec~ batch.write~ rec.file~ player~ gran.player~ pvoc.player~ pvoc.live~ grain.live~ grain.sampler~}}
    {control:\ MIDI
        {midi.learn sysrt.in sysrt.out ctl.in ctl.out touch.in touch.out pgm.in pgm.out bend.in bend.out note.in note.out midi.clock noteinfo panic mono voices suspedal keymap}}
    {control:\ OSC
        {osc.route osc.format osc.parse osc.send osc.receive}}
    {control:\ (mouse/keyboard)
        {mouse canvas.mouse keycode}}
    {control:\ (fade/pan/route)
        {fader~ autofade~ autofade2~ balance~ pan2~ pan4~ pan8~ spread~ rotate~ xfade~ xgate~ xgate2~ xselect~ xselect2~ mtx~}}
    {control:\ sequencers
        {euclid score score2 pattern sequencer sequencer~ impseq~ rec rec2}}
    {control:\ envelopes
        {adsr~ asr~ decay~ decay2~ envelope~ envgen~}}
    {control:\ lfo
        {phasor pimp lfo impulse pulse}}
    {control\ random/stochastic
        {rand.f rand.f~ rand.i rand.i~ rand.list rand.u rand.dist histogram rand.hist brown drunkard drunkard~ randpulse randpulse2 rampnoise lfnoise markov}}
    {control:\ line\ generators
        {function~ ramp~ glide~ glide2~ glide glide2 slew slew2 slew~ slew2~ lag~ lag2~ susloop~}}
    {control:\ triggers
        {above above~ bangdiv chance dust~ dust2~ gatehold~ gate2imp~ pimp~ pimpmul~ pulsecount~ pulsediv~ sh~ schmitt schmitt~ status status~ trig.delay~ trig.delay2~ toggleff~ timed.gate~ timed.gate match~ trig2bang~ trig2bang trighold~}}
    {control:\ clocks
        {speed clock tempo tempo~ metronome polymetro polymetro~}}    
    {analysis
        {changed~ changed2~ detect~ lastvalue median~ range range~ tap peak~ rms~ mov.rms~ vu~ zerocross~ beat~}}
}
