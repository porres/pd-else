local lua_sig = pd.Class:new():register("lua_sig")

function lua_sig:initialize(sel, atoms)
    self.inlets = {SIGNAL}
    self.outlets = {SIGNAL}
    self.phase = 0
    self.freq = 220
    self.amp = 0.5
    return true
end

-- dsp method is called when processing is about to start
function lua_sig:dsp(samplerate, blocksize)
    self.samplerate = samplerate
end

-- message to set frequency...
function lua_sig:in_1_freq(atoms)
    self.freq = atoms[1]
end

-- ... and amplitude.
function lua_sig:in_1_amp(atoms)
    self.amp = atoms[1]
end

-- perform method gets called with a table for each signal inlet
-- you must return a table for each signal outlet
function lua_sig:perform(in1)
    local frequency = self.freq   -- frequency of the sine wave in Hz
    local amplitude = self.amp    -- amplitude of the sine wave (0 to 1)

    -- Calculate the angular frequency (in radians per sample)
    local angular_freq = 2 * math.pi * frequency / self.samplerate

    -- Loop through each sample index
    for i = 1, #in1 do
       -- NOTE: We ignore the input signal in this example. Try multiplying by
       -- in1[i] for an amplitude/ring modulation effect.
        in1[i] = amplitude * math.sin(self.phase)
        self.phase = self.phase + angular_freq
        if self.phase >= 2 * math.pi then
            self.phase = self.phase - 2 * math.pi
        end
    end

    return in1
end
