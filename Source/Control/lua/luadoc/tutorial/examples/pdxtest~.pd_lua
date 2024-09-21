local pdxtest = pd.Class:new():register("pdxtest~")

-- This is intended as a playgound for the livecoding features in the latest
-- pdx.lua version. In particular, the new prerelod and postreload methods can
-- now be used to modify the number of inlets and outlets of an object on the
-- fly.

function pdxtest:initialize(sel, atoms)
   -- change these around and reload!
   self.inlets = {DATA,SIGNAL,SIGNAL}
   self.outlets = {SIGNAL,SIGNAL,DATA}
   self.phase = 0
   self.cycle = 0
   self.freq  = 133
   return true
end

function pdxtest:prereload()
   -- stuff to do pre-reload goes here
   pd.post("About to reload!")
end

function pdxtest:postreload()
   -- stuff to do post-reload goes here
   pd.post("Reloaded!")
   -- instead of doing a full initialization, you could also just change the
   -- number of inlets and outlets here
   self:initialize()
end

-- we keep this generic so that you can put the data inlets and outlets
-- wherever you please
function pdxtest:in_n_float(n, x)
   -- show the value we got, along with the inlet index
   pd.post(string.format("float: #%d: %g", n, x))
   -- send the data to whatever data outlets we have
   if type(self.outlets) == "number" then
      -- all data outlets
      for i = 1, self.outlets do
	 self:outlet(i, "float", {x})
      end
   elseif type(self.outlets) == "table" then
      -- full signature, need to look for DATA outlets
      for i = 1, #self.outlets do
	 if self.outlets[i] == DATA then
	    self:outlet(i, "float", {x})
	 end
      end
   end
end

function pdxtest:dsp(samplerate, blocksize)
   self.samplerate = samplerate
   self.blocksize = blocksize
end

function pdxtest:random_walk(dmin, dmax)
   -- random walk of sine waves with varying frequencies (cf. foo~.pd_lua)
   self.cycle = self.cycle + 1
   if self.cycle > 100 then
      self.freq = self.freq + math.random(-1, 1)*math.random(dmin, dmax)
      self.freq = math.min(1000, math.max(150, self.freq))
      self.cycle = 0
   end
end

function pdxtest:perform(in1, in2)
   local dmin, dmax = 50, 100
   self:random_walk(dmin, dmax)
   local freq, a, b = self.freq, 0.3, 0.1
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
   -- return up to two extra signals straight from the input
   return out, in1, in2
end
