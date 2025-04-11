local localsend = pd.Class:new():register("localsend")

function localsend:initialize(sel, atoms)
   self.inlets = 1
   -- pass the symbol from the creation argument,
   -- which gets automatically expanded here
   self.sender = tostring(atoms[1])
   return true
end

function localsend:in_1_sender(x)
   local sendername = tostring(x[1])

   -- store the original name as argument (like "\$0-foo")
   self:set_args({sendername})

   -- apply the expanded name with the local id
   self.sender = self:canvas_realizedollar(sendername)
end

function localsend:in_1_bang()
   pd.send(self.sender, "bang", {})
end
