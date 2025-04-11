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
pd._pathnames = { } -- look up absolute path by creation name
pd._objects = { }
pd._clocks = { }
pd._receives = { }
pd._loadpath = ""

-- add a path to Lua's "require" search paths
pd._setrequirepath = function(path)
  pd._packagepath = package.path
  pd._packagecpath = package.cpath
  if (pd._iswindows) then
    package.path = path .. "\\?.lua;" .. path .. "\\?\\init.lua;" .. package.path
    package.cpath = path .. "\\?.dll;" .. package.cpath
  else
    package.path = path .. "/?.lua;" .. path .. "/?/init.lua;" .. package.path
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

pd._dsp = function (object, samplerate, blocksize, inchans)
  local obj = pd._objects[object]
  if nil ~= obj and type(obj.dsp) == "function" then
    pd._objects[object]:dsp(samplerate, blocksize, inchans)
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
    obj:repaint(0)
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
      -- ag 20240906: record the clock in the target's _clocks table so that it
      -- can be destroyed automatically with the object
      if not object._clocks then
        object._clocks = { }
      end
      object._clocks[self._clock] = self
      return self
    end
  end
  return nil
end

function pd.Clock:destruct()
  -- ag 20240906: remove the clock from the target's _clocks table if any
  if self._target and self._target._clocks then
    --pd.post(string.format("%s: destroying clock %s", self._target._name, self._method))
    self._target._clocks[self._clock] = nil
  end
  if self._clock then
    pd._clocks[self._clock] = nil
    pd._clockfree(self._clock)
    self._clock = nil
  end
end

function pd.Clock:dispatch()
  if pd._objects[self._target] then
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
      -- ag 20240906: record the receiver in the target's _receives table so
      -- that it can be destroyed automatically with the object
      if not object._receives then
        object._receives = { }
      end
      object._receives[self._receive] = self
      return self
    end
  end
  return nil
end

function pd.Receive:destruct()
  -- ag 20240906: remove the receiver from the target's _receives table if any
  if self._target and self._target._receives then
    --pd.post(string.format("%s: destroying receiver %s", self._target._name, self._method))
    self._target._receives[self._receive] = nil
  end
  if self._receive then
    pd._receives[self._receive] = nil
    pd._receivefree(self._receive)
    self._receive = nil
    self._name = nil
    self._target = nil
    self._method = nil
  end
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
  --[[
     Oh well, it appears that the "temporary kludge" is here to stay, so
     let's at least fix it up to preserve an empty _loadpath, which is what we
     get with pdluax. Which makes sense, since pdluax's _scriptname will be an
     absolute path. But then an empty _loadpath should stay empty so that
     self._loadpath .. self._scriptname returns a proper path.

     This whole code has been touched so many times during sebshader's quest
     to get relative paths in object names right, that I don't really dare to
     touch it anymore, but this much we can do. -ag 20240905
  ]]
  local fullpath = string.sub(pd._loadpath, -1) == "/" and pd._loadpath or
     pd._loadpath ~= "" and pd._loadpath .. '/' or ""
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
  pd._pathnames[regname] = fullname
  pd._classes[fullname] = self       -- record registration
  self._class, self._class_gfx = pd._register(name)  -- register new class
  self._name = name
  self._loadpath = fullpath
  if name == "pdlua" then
    self._scriptname = "pd.lua"
  else
    self._scriptname = name .. ".pd_lua"
  end -- mrpeach 20111027
  return self                       -- return new
end

function pd.Class:construct(sel, atoms)
  self._object = pd._create(self._class, self._class_gfx)
  self.inlets = 0
  self.outlets = 0
  self._canvaspath = pd._canvaspath(self._object) .. "/"
  if pdx then
    pdx.reload(self)
  end
  if self:initialize(sel, atoms) then
    pd._createinlets(self._object, self.inlets)
    pd._createoutlets(self._object, self.outlets)
    if type(self.paint) == "function" then
        pd._creategui(self._object)
    end
    self:postinitialize()
    return self
  else
    if pdx then
      pdx.unreload(self)
    end
    return nil
  end
end

function pd.Class:destruct()
  -- ag 20240906: get rid of all clocks and receivers registered for us
  if self._clocks then
    local clocks = { }
    -- since the destruct() method destructively updates our _clocks table,
    -- record all registered clock objects in a new table
    for _, c in pairs(self._clocks) do
      table.insert(clocks, c)
    end
    -- now destroy them
    for _, c in ipairs(clocks) do
      c:destruct()
    end
    self._clocks = nil
  end
  if self._receives then
    local receives = { }
    -- since the destruct() method destructively updates our _receives table,
    -- record all registered receivers in a new table
    for _, r in pairs(self._receives) do
      table.insert(receives, r)
    end
    -- now destroy them
    for _, r in ipairs(receives) do
      r:destruct()
    end
    self._receives = nil
  end
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

function pd.Class:get_args()
  return pd._get_args(self._object)
end

function pd.Class:set_args(args)
  pd._set_args(self._object, args)
end

function pd.Class:canvas_realizedollar(s)
  return pd._canvas_realizedollar(self._object, s)
end

function pd.Class:signal_setmultiout(n, channelcount)
  pd._signal_setmultiout(self._object, n, channelcount)
end

function pd.Class:repaint(layer)
  -- do some argument checking first
  if type(layer) == "number" then
    -- make sure that layer is a nonnegative integer
    layer = math.max(0, math.floor(layer))
  elseif layer then
    self:error(string.format("repaint: invalid layer %s, must be number or nil", tostring(layer)))
    return
  end
  if layer == nil or layer <= 1 then
    if type(self.paint) == "function" then
      local g = _gfx_internal.start_paint(self._object, 1);
      if type(self.paint) == "function" and g ~= nil then
        self:paint(g)
        _gfx_internal.end_paint(g, 1)
      end
    end
  end
  if layer == nil or layer == 0 then
    local i = 2
    while true do
      local paint_layer_method = "paint_layer_" .. tostring(i)
      if type(self[paint_layer_method]) == "function" then
        local g = _gfx_internal.start_paint(self._object, i)
        if g ~= nil then
          self[paint_layer_method](self, g)
          _gfx_internal.end_paint(g, i)
          i = i + 1
        else
          break
        end
      else
        break -- Exit the loop when no more paint_layer_X methods are found
      end
    end
  end
  if layer ~= nil and layer >= 2 then
    local paint_layer_method = "paint_layer_" .. tostring(layer)
    if type(self[paint_layer_method]) == "function" then
      local g = _gfx_internal.start_paint(self._object, layer)
      if g ~= nil then
        self[paint_layer_method](self, g)
        _gfx_internal.end_paint(g, layer)
      end
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

function pd.Class:get_class() -- accessor for t_class*
  -- ag 20240905: this is now implemented on the C side (you probably
  -- shouldn't use this any more, we only keep this here for backward
  -- compatibility)
  return pd._get_class(self) or nil
end

local lua = pd.Class:new():register("pdlua")  -- global controls (the [pdlua] object only)

function lua:initialize(sel, atoms)
  self.inlets = 1
  self.outlets = 0    -- FIXME: might be nice to have errors go here?
  return true
end

function lua:in_1_load(atoms)  -- execute a script
  self:dofile(atoms[1])
end


local luax = pd.Class:new():register("pdluax")  -- classless lua externals (like [pdluax foo])

function luax:initialize(sel, atoms)          -- motivation: pd-list 2007-09-23
  if not atoms[1] then
    -- create a dummy object, which can still be clicked for help
    self.inlets = 0
    self.outlets = 0
    self._scriptname = ""
    return true
  end
  local f, pathname = self:dofile(atoms[1] .. ".pd_luax")
  if f and pathname then
    local function basename(str)
      return string.gsub(str, "(.*/)(.*)", "%2")
    end
    self._scriptname = pathname .. '/' .. basename(atoms[1]) .. ".pd_luax" -- mrpeach 20120201
    local atomstail = { }          -- munge for better lua<->luax compatibility
    for i,_ in ipairs(atoms) do
      if i > 1 then
        atomstail[i-1] = atoms[i]
      end
    end
    return f(self, atoms[1], atomstail)
  else
    return false   -- error message already output by dofile()
  end
end

-- convenience creation functions for classes, arrays, clocks, receivers

function pd.class(...)
   return pd.Class:new():register(...)
end

function pd.table(...)
   return pd.Table:new():sync(...)
end

function pd.clock(...)
   return pd.Clock:new():register(...)
end

function pd.receive(...)
   return pd.Receive:new():register(...)
end

-- constants used in the signal and graphics API
DATA = 0
SIGNAL = 1
Colors = {background = 0, foreground = 1, outline = 2}

-- pre-load pdx.lua (advanced live coding support); if you don't want this,
-- just comment out the line below
pdx = require 'pdx'

-- fin pd.lua
