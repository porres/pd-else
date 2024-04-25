--[[
pdlua -- a Lua embedding for Pd
Copyright (C) 2007,2008 Claude Heiland-Allen <claude@mathr.co.uk>
Copyright (C) 2012 Martin Peach martin.peach@sympatico.ca

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
--]]

-- storage for Pd C<->Lua interaction
pd._classes = { } -- take absolute paths and turn them into classes
pd._fullpaths = { }
pd._pathnames = { } -- look up absolute path by creation name
pd._objects = { }
pd._clocks = { }
pd._receives = { }
pd._loadpath = ""
pd._currentpath = ""

-- add a path to Lua's "require" search paths
pd._setrequirepath = function(path)
  pd._packagepath = package.path
  pd._packagecpath = package.cpath
  if (pd._iswindows) then
    package.path = path .. "\\?;" .. path .. "\\?.lua;" .. package.path
    package.cpath = path .. "\\?.dll;" .. package.cpath
  else
    package.path = path .. "/?;" .. path .. "/?.lua;" .. package.path
    package.cpath = path .. "/?.so;" .. package.cpath
  end
end

-- reset Lua's "require" search paths
pd._clearrequirepath = function()
  package.path = pd._packagepath
  package.cpath = pd._packagecpath
end

-- check if we need to register a basename class first
pd._checkbase = function (name)
  return pd._pathnames[name] == true
end

-- constructor dispatcher
pd._constructor = function (name, atoms)
  local fullpath = pd._pathnames[name]
  if nil ~= pd._classes[fullpath] then
    local o = pd._classes[fullpath]:new():construct(name, atoms)
    if o then
      pd._objects[o._object] = o
      return o._object
    end
  end
  return nil
end

-- destructor dispatcher
pd._destructor = function (object)
  if nil ~= pd._objects[object] then
    pd._objects[object]:destruct()
  end
end

-- inlet method dispatcher
pd._dispatcher = function (object, inlet, sel, atoms)
  if nil ~= pd._objects[object] then
    pd._objects[object]:dispatch(inlet, sel, atoms)
  end
end

pd._dsp = function (object, samplerate, blocksize)
  local obj = pd._objects[object]
  if nil ~= obj and type(obj.dsp) == "function" then
    pd._objects[object]:dsp(samplerate, blocksize)
  end
end

pd._perform_dsp = function (object, ...)
  local obj = pd._objects[object]
  if nil ~= obj and type(obj.perform) == "function" then
    return pd._objects[object]:perform(...)
  end
end

-- repaint method dispatcher
pd._repaint = function (object)
  local obj = pd._objects[object]
  if nil ~= obj and type(obj.repaint) == "function" then
    obj:repaint();
  end
end

-- mouse event dispatcher
pd._mouseevent = function (object, x, y, event_type)
  if nil ~= pd._objects[object] then
    local obj = pd._objects[object]
    if event_type == 0 and type(obj.mouse_down) == "function" then
      obj:mouse_down(x, y)
    end
    if event_type == 1 and type(obj.mouse_up) == "function" then
      obj:mouse_up(x, y)
    end
    if event_type == 2 and type(obj.mouse_move) == "function" then
      obj:mouse_move(x, y)
    end
    if event_type == 3 and type(obj.mouse_drag) == "function" then
      obj:mouse_drag(x, y)
    end
  end
end

-- clock method dispatcher
pd._clockdispatch = function (c)
  if nil ~= pd._clocks[c] then
    local m = pd._clocks[c]._method
    pd._clocks[c]._target[m](pd._clocks[c]._target)
  end
end

--whoami method dispatcher
pd._whoami = function (object)
  if nil ~= pd._objects[object] then
    return pd._objects[object]:whoami()
  end
end

--whereami method dispatcher
pd._whereami = function(name)
    if nil ~= pd._fullpaths[name] then
      return pd._fullpaths[name]
    end

    return nil
end

--class method dispatcher
pd._get_class = function (object)
  if nil ~= pd._objects[object] then
    return pd._objects[object]:get_class()
  end
end

-- prototypical OO system
pd.Prototype = { }

function pd.Prototype:new(o)
  o = o or {}
  setmetatable(o, self)
  self.__index = self
  return o
end

-- clocks
pd.Clock = pd.Prototype:new()

function pd.Clock:register(object, method)
  if nil ~= object then
    if nil ~= object._object then
      self._clock = pd._createclock(object._object, method)
      self._target = object
      self._method = method
      pd._clocks[self._clock] = self
      return self
    end
  end
  return nil
end

function pd.Clock:destruct()
  pd._clocks[self._clock] = nil
  pd._clockfree(self._clock)
  self._clock = nil
end

function pd.Clock:dispatch()
  local m = self._target[self._method]
  if type(m) == "function" then
    return m(self._target)
  else
    self._target:error(
      "no method for `" .. self._method ..
      "' at clock of Lua object `" .. self._name .. "'"
    )
  end
end

function pd.Clock:set(systime)
  pd._clockset(self._clock, systime)
end

function pd.Clock:delay(delaytime)
  pd._clockdelay(self._clock, delaytime)
end

function pd.Clock:unset()
  pd._clockunset(self._clock)
end

-- tables
pd.Table = pd.Prototype:new()

function pd.Table:sync(name)
  self.name = name
  self._length, self._array = pd._getarray(name)
  if self._length < 0 then
    return nil
  else
    return self
  end
end

function pd.Table:destruct()
  self._length = -3
  self._array = nil
end

function pd.Table:get(i)
  if type(i) == "number" and 0 <= i and i < self._length then
    return pd._readarray(self._length, self._array, i)
  else
    return nil
  end
end

function pd.Table:set(i, f)
  if type(i) == "number" and type(f) == "number" and 0 <= i and i < self._length then
    return pd._writearray(self._length, self._array, i, f)
  else
    return nil
  end
end

function pd.Table:length()
  if self._length >= 0 then
    return self._length
  else
    return nil
  end
end

function pd.Table:redraw()
  pd._redrawarray(self.name)
end

-- receivers
function pd._receivedispatch(receive, sel, atoms)
  if nil ~= pd._receives[receive] then
    pd._receives[receive]:dispatch(sel, atoms)
  end
end

pd.Receive = pd.Prototype:new()

function pd.Receive:register(object, name, method)
  if nil ~= object then
    if nil ~= object._object then
      self._receive = pd._createreceive(object._object, name)
      self._name = name
      self._target = object
      self._method = method
      pd._receives[self._receive] = self
      return self
    end
  end
  return nil
end

function pd.Receive:destruct()
  pd._receives[self._receive] = nil
  pd._receivefree(self._receive)
  self._receive = nil
  self._name = nil
  self._target = nil
  self._method = nil
end

function pd.Receive:dispatch(sel, atoms)
  self._target[self._method](self._target, sel, atoms)
end

-- patchable objects
pd.Class = pd.Prototype:new()

function pd.Class:register(name)
  -- if already registered, return existing
  local regname
  -- Those trailing slashes keep piling up, thus we need to check whether
  -- pd._loadpath already has one. This is only a temporary kludge until a
  -- proper fix is deployed. -ag 2023-02-02
  local fullpath = string.sub(pd._loadpath, -1) == "/" and pd._loadpath or
     (pd._loadpath .. '/')
  local fullname = fullpath .. name

  if nil ~= pd._classes[fullname] then
    return pd._classes[fullname]
  end
  if pd._loadname then
    -- don't alter existing classes of basename,
    -- if another file has ownership of basename
    if not pd._pathnames[name] then
      pd._pathnames[name] = true
    end
    regname = pd._loadname
  else
    regname = name
  end

  --pd._fullpaths[regname] = pd._currentpath or (fullname .. ".pd_lua")
  if pd._currentpath == nil or pd._currentpath == '' then
    pd._fullpaths[regname] = fullname .. ".pd_lua"
  else
    pd._fullpaths[regname] = pd._currentpath
  end

  pd._pathnames[regname] = fullname
  pd._classes[fullname] = self       -- record registration
  self._class = pd._register(name)  -- register new class
  self._name = name
  self._path = pd._fullpaths[regname]
  self._loadpath = fullpath
  if name == "pdlua" then
    self._scriptname = "pd.lua"
  else
    self._scriptname = name .. ".pd_lua"
  end -- mrpeach 20111027
  return self                       -- return new
end

function pd.Class:construct(sel, atoms)
  self._object = pd._create(self._class)
  self.inlets = 0
  self.outlets = 0
  self._canvaspath = pd._canvaspath(self._object) .. "/"
  if self:initialize(sel, atoms) then
    pd._createinlets(self._object, self.inlets)
    pd._createoutlets(self._object, self.outlets)
    if type(self.paint) == "function" then
        pd._creategui(self._object)
    end
    self:postinitialize()
    return self
  else
    return nil
  end
end

function pd.Class:destruct()
  pd._objects[self] = nil
  self:finalize()
  pd._destroy(self._object)
end

function pd.Class:dispatch(inlet, sel, atoms)
  local m = self[string.format("in_%d_%s", inlet, sel)]
  if type(m) == "function" then
    if sel == "bang"    then return m(self)           end
    if sel == "float"   then return m(self, atoms[1]) end
    if sel == "symbol"  then return m(self, atoms[1]) end
    if sel == "pointer" then return m(self, atoms[1]) end
    if sel == "list"    then return m(self, atoms)    end
    return m(self, atoms)
  end
  m = self["in_n_" .. sel]
  if type(m) == "function" then
    if sel == "bang"    then return m(self, inlet)           end
    if sel == "float"   then return m(self, inlet, atoms[1]) end
    if sel == "symbol"  then return m(self, inlet, atoms[1]) end
    if sel == "pointer" then return m(self, inlet, atoms[1]) end
    if sel == "list"    then return m(self, inlet, atoms)    end
    return m(self, inlet, atoms)
  end
  m = self[string.format("in_%d", inlet)]
  if type(m) == "function" then
    return m(self, sel, atoms)
  end
  m = self["in_n"]
  if type(m) == "function" then
    return m(self, inlet, sel, atoms)
  end
  self:error(
     string.format("no method for `%s' at inlet %d of Lua object `%s'",
		   sel, inlet, self._name)
  )
end

function pd.Class:outlet(outlet, sel, atoms)
  pd._outlet(self._object, outlet, sel, atoms)
end

function pd.Class:initialize(sel, atoms) end

function pd.Class:postinitialize() end

function pd.Class:finalize() end

function pd.Class:set_args(args)
    pd._set_args(self._object, args)
end

function pd.Class:repaint()
  if type(self.paint) == "function" then
    local g = _gfx_internal.start_paint(self._object);
    if g ~= nil then
        self:paint(g);
        _gfx_internal.end_paint(g);
    end
  end
end

function pd.Class:get_size()
    return _gfx_internal.get_size(self._object)
end

function pd.Class:set_size(width, height)
    return _gfx_internal.set_size(self._object, width, height)
end

function pd.Class:dofilex(file)
  -- in case of register being called, make sure
  -- classes in other paths aren't getting affected
  -- save old loadname in case of weird nesting loading
  local namesave = pd._loadname
  local pathsave = pd._loadpath
  pd._loadname = nil
  pd._loadpath = self._loadpath
  pd._currentpath = file
  local f, path = pd._dofilex(self._class, file)
  pd._loadname = namesave
  pd._loadpath = pathsave
  return f, path
end

function pd.Class:dofile(file)
  -- in case of register being called, make sure
  -- classes in other paths aren't getting affected
  -- save old loadname in case of weird nesting loading
  local namesave = pd._loadname
  local pathsave = pd._loadpath
  pd._loadname = nil
  pd._loadpath = self._loadpath
  pd._currentpath = file
  local f, path = pd._dofile(self._object, file)
  pd._loadname = namesave
  pd._loadpath = pathsave
  return f, path
end

function pd.Class:error(msg)
  pd._error(self._object, msg)
end

function pd.Class:whoami()
  return self._scriptname or self._name
end

function pd.Class:in_1__reload()
  self:dofile(self._path)
end

function pd.Class:get_class() -- accessor for t_class*
  return self._class or nil
end


local lua = pd.Class:new():register("lua")  -- global controls (the [pdlua] object only)

function lua:initialize(sel, atoms)
  self.inlets = {}
  self.outlets = {}
  self.gui = 0

  local code_start = 0

  local i = 1
  while i <= #atoms do
      local atom = atoms[i]
      if i > 1 and atom == ";" then
         code_start = i;
         break  -- Stop if we reach the first occurrence of ";"
      end

      if type(atom) == "string" then
          if atom == "-in" and i < #atoms and type(atoms[i+1]) == "number" then
              local in_count = atoms[i+1]
              for _ = 1, in_count do
                  table.insert(self.inlets, DATA)
              end
              i = i + 1 -- Skip the next element since we've used it
          elseif atom == "-sigin" and i < #atoms and type(atoms[i+1]) == "number" then
              local sigin_count = atoms[i+1]
              for _ = 1, sigin_count do
                  table.insert(self.inlets, SIGNAL, 1)
              end
              i = i + 1 -- Skip the next element since we've used it
          elseif atom == "-out" and i < #atoms and type(atoms[i+1]) == "number" then
              local out_count = atoms[i+1]
              for _ = 1, out_count do
                  table.insert(self.outlets, DATA)
              end
              i = i + 1 -- Skip the next element since we've used it
            elseif atom == "-sigout" and i < #atoms and type(atoms[i+1]) == "number" then
              local sigout_count = atoms[i+1]
              for _ = 1, sigout_count do
                 table.insert(self.outlets, SIGNAL, 1)
              end
              i = i + 1 -- Skip the next element since we've used it
            end
        end
        i = i + 1
    end

  for _ = 1, code_start do
      table.remove(atoms, 1)
  end

  -- Concatenate atoms into a single string separated by spaces
  local lua_code = table.concat(atoms, " ")
  lua_code = string.gsub(lua_code, ";", "\n")

  self.function_prefix = "fn_" .. tostring(math.random(0, 1<<32)) .. "_"

  -- Give functions unique names to prevent namespace clashes
  lua_code = string.gsub(lua_code, "function%s+in_(%d+)_(%a+)", function(num, type)
          return "function " .. self.function_prefix .. "in_" .. num .. "_" .. type
      end)
  lua_code = string.gsub(lua_code, "function%s+in_n_(%a+)", function(type)
          return "function " .. self.function_prefix .. "in_n_" .. type
      end)
  lua_code = string.gsub(lua_code, "function%s+in_(%d+)", function(num)
          return "function " .. self.function_prefix .. "in_" .. num
      end)
  lua_code = string.gsub(lua_code, "function%s+in_n", function()
          return "function " .. self.function_prefix .. "in_n"
      end)
  lua_code = string.gsub(lua_code, "function%s$0", function()
          return "function " .. self.function_prefix
      end)
  lua_code = string.gsub(lua_code, "function%sdsp", function()
            return "function " .. self.function_prefix .. "dsp"
        end)
  lua_code = string.gsub(lua_code, "function%sperform", function()
            return "function " .. self.function_prefix .. "perform"
        end)

  -- Create a temporary file
  self.temp_name = os.tmpname()
  local temp_file = io.open(self.temp_name, 'w+b')

  if temp_file then
      -- Writing the concatenated string to the temp file
      temp_file:write(lua_code)

      -- It's important to close the file when you're done
      temp_file:close()

      self:dofile(self.temp_name)
  else
      pd.post("Error: could not create temp file")
  end

  return true
end

function lua:dsp(sample_rate, block_size)
    local m = _G[self.function_prefix .. "dsp"]
    if type(m) == "function" then
    return m(self, sample_rate, block_size)
    end
end

function lua:perform(...)
    local m = _G[self.function_prefix .. "perform"]
    if type(m) == "function" then
    return m(self, ...)
    end
end

function lua:dispatch(inlet, sel, atoms)
  if sel == "load" then
       self:dofile(atoms[1])
       return
  end

  local m = _G[self.function_prefix .. string.format("in_%d_%s", inlet, sel)]
  if type(m) == "function" then
    if sel == "bang"    then return m(self)           end
    if sel == "float"   then return m(self, atoms[1]) end
    if sel == "symbol"  then return m(self, atoms[1]) end
    if sel == "pointer" then return m(self, atoms[1]) end
    if sel == "list"    then return m(self, atoms)    end
    return m(self, atoms)
  end
  m = self[self.function_prefix .. "in_n_" .. sel]
  if type(m) == "function" then
    if sel == "bang"    then return m(self, inlet)           end
    if sel == "float"   then return m(self, inlet, atoms[1]) end
    if sel == "symbol"  then return m(self, inlet, atoms[1]) end
    if sel == "pointer" then return m(self, inlet, atoms[1]) end
    if sel == "list"    then return m(self, inlet, atoms)    end
    return m(self, inlet, atoms)
  end
  m = self[self.function_prefix .. string.format("in_%d", inlet)]
  if type(m) == "function" then
    return m(self, sel, atoms)
  end
  m = self[self.function_prefix .. "in_n"]
  if type(m) == "function" then
    return m(self, inlet, sel, atoms)
  end
  self:error(
     string.format("no method for `%s' at inlet %d of Lua object `%s'",
		   sel, inlet, self._name)
  )
end

function lua:__gc()
    os.remove(self.temp_name)
end

DATA = 0
SIGNAL = 1
Colors = {background = 0, foreground = 1, outline = 2}
-- fin pd.lua
