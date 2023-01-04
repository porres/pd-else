local luasend = pd.Class:new():register("luasend")

function luasend:initialize(sel, atoms)
   self.inlets = 1
   -- pass the receiver symbol as creation argument
   self.receiver = tostring(atoms[1])
   pd.post(string.format("luasend: receiver '%s'", self.receiver))
   return true
end

function luasend:in_1_bang()
   pd.send(self.receiver, "float", {1})
end
