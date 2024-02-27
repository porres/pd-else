local fibs = pd.Class:new():register("fibs")

function fibs:initialize(sel, atoms)
   -- one inlet for bangs and other messages
   self.inlets = 1
   -- two outlets for the numbers in pairs
   self.outlets = 2
   -- intial pair
   self.a, self.b = 0, 1
   -- the modulus can also be set as creation argument
   self.m = type(atoms[1]) == "number" and atoms[1] or 10
   -- make sure that it's an integer > 0
   self.m = math.max(1, math.floor(self.m))
   -- print the modulus in the console, so that the user knows what it is
   pd.post(string.format("fibs: modulus %d", self.m))
   return true
end

function fibs:in_1_bang()
   -- output the current pair in the conventional right-to-left order
   self:outlet(2, "float", {self.b})
   self:outlet(1, "float", {self.a})
   -- calculate the next pair; note that it's sufficient to calculate the
   -- remainder for the new number
   self.a, self.b = self.b, (self.a+self.b) % self.m
end

function fibs:in_1_float(m)
   -- a float input changes the modulus and resets the sequence
   self.m = math.max(1, math.floor(m))
   self.a, self.b = 0, 1
   pd.post(string.format("fibs: modulus %d", self.m))
end

function fibs:in_1_reset()
   -- a reset message just resets the sequence
   self.a, self.b = 0, 1
end
