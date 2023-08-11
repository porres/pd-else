# generate menu tree for native objects for the canvas right click popup
# code by Porres and Seb Shader

package require pd_menus

namespace eval category_else_menu {
}

proc menu_send_else_obj {w x y item} {
    if {$item eq "else"} {  
        pdsend "$w obj $x $y $item"
    } else {
        pdsend "$w obj $x $y else/$item"
#        pdsend "$w obj $x $y $item"
    } 
}

# set nested list
proc category_else_menu::load_menutree {} {
    set menutree { 
        {else
            {assorted
                {else}}
            {gui
                {knob numbox~ drum.seq bicoeff pad messbox mtx.ctl biplot zbiplot pic colors function circle slider2d display out.mc~ out~ out4~ out8~ gain~ gain2~ button keyboard graph~ range.hsl multi.vsl spectrograph~ meter~ meter2~ meter4~ meter8~ note mix2~ mix4~ setdsp~ openfile oscope~}}
            {time
                {chrono datetime}}
            {fft
                {hann~ bin.shift~}}
            {table
                {buffer tabgen tabreader tabreader~}}
            {tuning/notes
                {scales scale2freq scala autotune autotune2 makenote2 retune eqdiv cents2scale scale2cents cents2frac frac2cents dec2frac frac2dec freq2midi midi2freq note2pitch pitch2note note2dur}}
            {patch/subpatch\ management
                {loadbanger args meter presets dollsym sender receiver retrieve blocksize~ click properties fontsize canvas.active canvas.bounds canvas.gop canvas.pos canvas.file canvas.edit canvas.vis canvas.name canvas.setname canvas.zoom}}
            {message\ management
                {format swap2 nmess unite separate symbol2any any2symbol changed hot initmess message default pack2 pick limit spread router route2 routeall routetype selector stack store morph interpolate sig2float~ float2sig~ pipe2}}
            {list\ management
                {break order combine delete group iterate insert scramble sort reverse rotate replace sum slice stream merge unmerge amean gmean}}
            {file\ management
                {dir}}
            {midi
                {midi midi.learn sysrt.in sysrt.out ctl.in ctl.out touch.in touch.out pgm.in pgm.out nemd.in bend.out note.in note.out midi.clock noteinfo panic mono voices suspedal}}
            {osc
                {osc.route osc.format osc.parse osc.send osc.receive}}
            {math\ functions
                {add add~ median avg mov.avg count gcd lcm frac.add frac.mul ceil ceil~ factor floor floor~ trun trunc~ rint rint~ quantizer quantizer~ fold fold~ lastvalue mag mag~ sin~ wrap2 wrap2~ op op~ cmul~}}
            {math\ conversion
                {hex2dec dec2hex bpm car2pol car2pol~ cents2ratio cents2ratio~ ms2samps ms2samps~ db2lin db2lin~ float2bits hz2rad lin2db lin2db~ deg2rad rad2deg pz2coeff coeff2pz rad2hz ratio2cents ratio2cents~ samps2ms samps2ms~ pol2car pol2car~ rescale rescale~}}
            {math\ constant\ values
                {sr~ nyquist~ pi e}}
            {logic
                {loop}}
            {audio\ multichannel\ tools
                {nchs~ sigs~ repeat~ select~ pick~ get~ sum~ merge~ unmerge~ slice~}}
            {fx\ assorted
                {downsample~ conv~ chorus~ shaper~ crusher~ drive~ power~ flanger~ freq.shift~ pitch.shift~ stretch.shift~ stretch.shift~ ping.pong~ rm~ tremolo~ vibrato~ vocoder~ morph~ freeze~ pvoc.freeze~ phaser~}}
            {fx\ delay
                {del\ in~ del\ out~ fbdelay~ ffdelay~ revdelay~ filterdelay~}}
            {fx\ dynamics
                {compress~ duck~ expand~ noisegate~ norm~}}
            {fx\ reverberation
                {allpass.rev~ comb.rev~ echo.rev~ mono.rev~ stereo.rev~ free.rev~ giga.rev~ plate.rev~ fdn.rev~}}
            {fx\ filters
                {allpass.2nd~ allpass.filt~ bitnormal~ comb.filt~ lop.bw~ hip.bw~ biquads~ bandpass~ bandstop~ crossover~ bpbank~ bicoeff2 brickwall~ eq~ highpass~ highshelf~ lop2~ lowpass~ lowshelf~ mov.avg~ resonbank~ resonbank2~ resonant~ resonant2~ svfilter~}}
            {sampling\ playing\ granulation
                {player~ gran.player~ pvoc.player~ pvoc.live~ batch.rec~ bach.write~ rec.file~ play.file~ tabplayer~ tabwriter~ sample~}}
            {synthesis:\ synthesizers
                {sfont~ sfz~ plaits~ synth~}}
            {synthesis:\ granular
                {grain.synth~}}
            {synthesis:\ physical\ modelling
                {pluck~}}
            {synthesis:\ oscillators
                {cosine~ impulse~ imp.mc~ impulse2~ parabolic~ pulse~ saw~ saw2~ oscbank~ oscbank2~ osc.mc~ oscnoise~ sine~ square~ tri~ gaussian~ vsaw~ pmosc~ wavetable~ blip~ bl.osc~ bl.imp~ bl.imp2~ bl.saw~ bl.saw2~ bl.square~ bl.tri~ bl.vsaw~ bl.wavetable~}}
            {synthesis:\ chaotic\ stochastic\ noise
                {white~ brown~ perlin~ crackle~ cusp~ fbsine~ fbsine2~ gbman~ gray~ henon~ ikeda~ latoocarfian~ lorenz~ lfnoise~ lincong~ logistic~ quad~ stepnoise~ stepnoise.mc~ rampnoise~ rampnoise.mc~ randpulse~ randpulse2~ standard~ pink~ xmod~ xmod2~ gendyn~}}
            {control:\ mouse\ keyboard
                {mouse canvas.mouse keycode keymap}}
            {control:\ fade\ pan\ routing
                {fader~ autofade~ autofade2~ balance~ pan2~ pan4~ pan8~ spread~ spread.mc~ rotate~ rotate.mc~ xfade~ xfade.mc~ xgate~ xgate.mc~ xgate2~ xselect~ xselect2~ xselect.mc~ mtx~}}
            {control:\ sequencers
                {euclid score score2 pattern sequencer sequencer~ phaseseq~ impseq~ rec rec2}}
            {control:\ envelopes
                {adsr~ asr~ decay~ decay2~ envelope~ envgen~}}
            {control:\ ramp\ line/curve\ generators
                {ramp~ susloop~ function~ slew slew~ slew2 slew2~ lag~ lag2~ glide glide~ glide2 glide2~}}
            {control:\ random/stochastic
                {rand.f rand.f~ rand.i rand.i~ rand.list rand.u rand.dist rand.hist histogram markov drunkard drunkard~ brown randpulse randpulse2 lfnoise stepnoise rampnoise}}
            {control:\ control\ rate\ lfo
                {lfo phasor pimp impulse pulse}}
            {control:\ triggers
                {above above~ bangdiv chance chance~ dust~ dust2~ gatehold~ gate2imp~ pimp~ pimpmul~ pulsecount~ pulsediv~ sh~ schmitt schmitt~ status status~ trig.delay~ trig.delay2~ toggleff~ timed.gate timed.gate~ match~ trig2bang trig2bang~ trighold~}}
            {control:\ triggers\ clock
                {clock metronome metronome~ polymetro polymetro~ speed tempo tempo~}}
            {analysis
                {changed~ changed2~ detect~ lastvalue~ median~ peak~ tap range range~ maxpeak~ rms~ mov.rms~ vu~ zerocross~ beat~}}
        }
    }
    return $menutree
}

proc category_else_menu::create {cmdstring code result op} {
    set mymenu [lindex $cmdstring 1]
    set x [lindex $cmdstring 3]
    set y [lindex $cmdstring 4]
    set menutree [load_menutree]
    $mymenu add separator
    foreach categorylist $menutree {
        set category [lindex $categorylist 0]
        menu $mymenu.$category
        $mymenu add cascade -label $category -menu $mymenu.$category
        foreach subcategorylist [lrange $categorylist 1 end] {
            set subcategory [lindex $subcategorylist 0]
            menu $mymenu.$category.$subcategory
            $mymenu.$category add cascade -label $subcategory -menu $mymenu.$category.$subcategory
            foreach item [lindex $subcategorylist end] {
                # replace the normal dash with a Unicode minus so that Tcl does not
                # interpret the dash in the -label to make it a separator
                $mymenu.$category.$subcategory add command \
                    -label [regsub -all {^\-$} $item {−}] \
                    -command "menu_send_else_obj \$::focused_window $x $y {$item}"
            }
        }
    }
}

trace add execution ::pdtk_canvas::create_popup leave category_else_menu::create
