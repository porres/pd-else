local foo = pd.Class:new():register("foo")

function foo:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   self.counter = 0
   self.step = 1
   if type(atoms[1]) == "number" then
      self.counter = atoms[1]
   elseif type(atoms[1]) ~= "nil" then
      self:error(string.format("foo: #1: %s is of the wrong type %s",
			       tostring(atoms[1]), type(atoms[1])))
   end
   if type(atoms[2]) == "number" then
      self.step = atoms[2]
   elseif type(atoms[2]) ~= "nil" then
      self:error(string.format("foo: #2: %s is of the wrong type %s",
			       tostring(atoms[2]), type(atoms[2])))
   end
   pd.post(string.format("foo: initialized counter: %g, step size: %g",
			 self.counter, self.step))
   return true
end

foo.init = 0

function foo:postinitialize()
   if foo.init == 0 then
      pd.post("Welcome to foo! Copyright (c) by Foo software.")
   end
   foo.init = foo.init + 1
end

function foo:finalize()
   foo.init = foo.init - 1
   if foo.init == 0 then
      pd.post("Thanks for using foo!")
   end
end

function foo:in_1_bang()
   self:outlet(1, "float", {self.counter})
   self.counter = self.counter + self.step
end
