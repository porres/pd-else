local lpaths = pd.Class:new():register("lpaths")

function lpaths:initialize(name, atoms)
  self.inlets = 1
  self.outlets = 1
  return true
end

function lpaths:in_1_loadx(atoms)
  local fname = self._loadpath .. "hello.txt"
  local f = assert(io.open(fname, "rb"))
  local content = f:read("*all")
  f:close()
  pd.post(content)
  return true
end

function lpaths:in_1_load(atoms)
  local fname = self._canvaspath .. "hello.txt"
  local f = assert(io.open(fname, "rb"))
  local content = f:read("*all")
  f:close()
  pd.post(content)
  return true
end

function lpaths:in_1_postpath(atoms)
  pd.post("self._canvaspath: " .. self._canvaspath)
end

function lpaths:in_1_postpathx(atoms)
  pd.post("self._loadpath: " .. self._loadpath)
end

