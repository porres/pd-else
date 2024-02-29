
--[[
pdx.lua: useful extensions to pd.lua
Copyright (C) 2020 Albert Gräf <aggraef@gmail.com>

To use this in your pd-lua scripts: local pdx = require 'pdx'

Currently there's only the pdx.reload() function, which implements a kind of
remote reload functionality based on dofile and receivers, as explained in the
pd-lua tutorial. More may be added in the future.

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

local pdx = {}

--[[
Reload functionality. Call pdx.reload() on your object to enable, and
pdx.unreload() to disable this functionality again.

pdx.reload installs a "pdluax" receiver which reloads the object's script file
when it receives the "reload" message without any arguments, or a "reload
class" message with a matching class name.

We have to go to some lengths here since we only want the script to be
reloaded *once* for each class, not for every object in the class. This
complicates things quite a bit. In particular, we need to keep track of object
deletions to perform the necessary bookkeeping, in order to ensure that at any
time there's exactly one participating object in the class which receives the
message. Which means that we also need to keep track of the object's finalize
method, so that we can chain to it in our own finalizer.
--]]

local reloadables = {}

-- finalize method, here we perform the necessary bookkeeping when an object
-- in the reloadables table gets deleted
local function finalize(self)
   if reloadables[self._name] then
      -- remove the object from the reloadables table, and restore its
      -- original finalizer
      pdx.unreload(self)
      -- call the object's own finalizer, if any
      if self.finalize then
	 self.finalize(self)
      end
   end
end

-- Our receiver. In the future, more functionality may be added here, but at
-- present this only recognizes the "reload" message and checks the class
-- name, if given.
local function pdluax(self, sel, atoms)
   if sel == "reload" then
      -- reload message, check that any extra argument matches the class name
      if atoms[1] == nil or atoms[1] == self._name then
	 pd.post(string.format("pdx: reloading %s", self._name))
	 self:dofilex(self._scriptname)
	 -- update the object's finalizer and restore our own, in case
	 -- anything has changed there
	 if self.finalize ~= finalize then
	    reloadables[self._name][self].finalize = self.finalize
	    self.finalize = finalize
	 end
      end
   end
end

-- purge an object from the reloadables table
function pdx.unreload(self)
   if reloadables[self._name] then
      if reloadables[self._name].current == self then
	 -- self is the current receiver, find another one
	 local current = nil
	 for obj, data in pairs(reloadables[self._name]) do
	    if type(obj) == "table" and obj ~= self then
	       -- install the receiver
	       data.recv = pd.Receive:new():register(obj, "pdluax", "_pdluax")
	       obj._pdluax = pdluax
	       -- record that we have a new receiver and bail out
	       current = obj
	       break
	    end
	 end
	 reloadables[self._name].current = current
	 -- get rid of the old receiver
	 reloadables[self._name][self].recv:destruct()
	 self._pdluax = nil
      end
      -- restore the object's finalize method
      self.finalize = reloadables[self._name][self].finalize
      -- purge the object from the reloadables table
      reloadables[self._name][self] = nil
      -- if the list of reloadables in this class is now empty, purge the
      -- entire class from the reloadables table
      if reloadables[self._name].current == nil then
	 reloadables[self._name] = nil
      end
   end
end

-- register a new object in the reloadables table
function pdx.reload(self)
   if reloadables[self._name] then
      -- We already have an object for this class, simply record the new one
      -- and install our finalizer so that we can perform the necessary
      -- cleanup when the object gets deleted. Also check that we don't
      -- register the same object twice (this won't really do any harm, but
      -- would be a waste of time).
      if reloadables[self._name][self] == nil then
	 reloadables[self._name][self] = { finalize = self.finalize }
	 self.finalize = finalize
      end
   else
      -- New class, make this the default receiver.
      reloadables[self._name] = { current = self }
      reloadables[self._name][self] = { finalize = self.finalize }
      -- install our finalizer
      self.finalize = finalize
      -- add the receiver
      reloadables[self._name][self].recv =
	 pd.Receive:new():register(self, "pdluax", "_pdluax")
      self._pdluax = pdluax
   end
end

return pdx
