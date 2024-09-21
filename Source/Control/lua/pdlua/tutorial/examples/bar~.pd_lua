local bar = pd.Class:new():register("bar~")

function bar:initialize(sel, atoms)
   self.outlets = {SIGNAL}
   self.phase = 0
   self.cycle = 0
   self.freq  = 233
   return true
end

function bar:dsp(samplerate, blocksize)
   self.samplerate = samplerate
   self.blocksize = blocksize
end

function bar:random_walk(dmin, dmax)
   self.cycle = self.cycle + 1
   if self.cycle > 100 then
      self.freq = self.freq + math.random(-1, 1)*math.random(dmin, dmax)
      self.freq = math.min(600, math.max(150, self.freq))
      self.cycle = 0
   end
end

function bar:perform()
   -- random frequency change
   local dmin, dmax = 30, 70
   self:random_walk(dmin, dmax)
   local freq, a, b = self.freq, 0.4, 0.08
   local out = {} -- result table
   -- random phase offset
   local d = b * math.random()
   for i = 1, self.blocksize do
      -- phase in radians
      local x = 2 * math.pi * (self.phase + d)
      out[i] = a * math.sin(x)
      self.phase = self.phase + freq / self.samplerate
      if self.phase >= 1 then
         self.phase = self.phase - 1
      end
   end
   return out
end
