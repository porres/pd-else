local luaxfade = pd.Class:new():register("luaxfade~")

function luaxfade:initialize(sel, atoms)
   self.inlets = {SIGNAL,SIGNAL}
   self.outlets = {SIGNAL}
   self.xfade = 0
   self.xfade_to = 0
   self.xfade_time = 0
   self.xfade_delay = 0
   self.time = 0
   self.ramp = 0
   return true
end

function luaxfade:dsp(samplerate, blocksize)
   self.samplerate = samplerate
end

function luaxfade:in_1_fade(atoms)
   -- If self.samplerate is not initialized, because the dsp method has not
   -- been run yet, then we cannot compute the sample delay and ramp times
   -- below, so we bail out, telling the user to enable dsp first.
   if not self.samplerate then
      self:error("luaxfade~: unknown sample rate, please enable dsp first")
      return
   end
   local fade, time, delay = table.unpack(atoms)
   if type(delay) == "number" then
      -- delay time (msec -> samples)
      self.xfade_delay = math.floor(self.samplerate*delay/1000)
   end
   if type(time) == "number" then
      -- xfade time (msec -> samples)
      self.xfade_time = math.floor(self.samplerate*time/1000)
   end
   if type(fade) == "number" then
      -- new xfade value (clamp to 0-1)
      self.xfade_to = math.max(0, math.min(1, fade))
   end
   if self.xfade_to ~= self.xfade then
      -- start a new cycle
      if self.xfade_delay > 0 then
         self.time, self.ramp = self.xfade_delay, 0
      elseif self.xfade_time > 0 then
         self.time, self.ramp = self.xfade_time, (self.xfade_to-self.xfade)/self.xfade_time
      else
         self.xfade = self.xfade_to
         self.time, self.ramp = 0, 0
      end
   end
end

function luaxfade:perform(in1, in2)
   local xfade, xfade_to = self.xfade, self.xfade_to
   local xfade_time = self.xfade_time
   local time, ramp = self.time, self.ramp

   -- loop through each sample index
   for i = 1, #in1 do
      -- mix (we do this in-place, using in1 for output)
      in1[i] = in1[i]*(1-xfade) + in2[i]*xfade
      -- update the mix if time > 0 (sample countdown)
      if time > 0 then
         -- update cycle is still in ptogress
         if ramp ~= 0 then
            xfade = xfade + ramp
         end
         time = time - 1
      elseif xfade_to ~= xfade then
         if xfade_time > 0 then
            -- start the ramp up or down
            time, ramp = xfade_time, (xfade_to-xfade)/xfade_time
         else
            -- no xfade_time, jump to the new value immediately
            xfade = xfade_to
         end
      end
   end

   -- update internal state
   self.xfade, self.time, self.ramp = xfade, time, ramp

   -- return the mixed down sample data
   return in1
end
