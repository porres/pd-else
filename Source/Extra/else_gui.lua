-- else_gui.lua
-- Reusable helpers for Pure Data Lua GUI objects.
--
-- Usage:
--   local else_gui = require("else_gui")
--   else_gui.setup(MyClass, ARGS, EXTRA_FLAGS)
local else_gui = {}

-- ─── Internal helpers ─────────────────────────────────────────────────────────

local function is_color(def) return def[5] == "color" end
local function is_range(def) return def[5] == "range" end
local function has_prop(def)  return def[4] ~= nil and def[5] ~= nil end

-- Number of creation-arg slots consumed by one def entry.
local function slot_count(def)
    if is_color(def) then return 3 end
    if is_range(def) then return 2 end
    return 1
end

-- Deep-copy a value (one level; enough for {r,g,b} tables).
local function clone(v)
    if type(v) ~= "table" then return v end
    local t = {}
    for k, kv in pairs(v) do t[k] = kv end
    return t
end

-- Evaluate a consume expression like "slots*tracks" against self's current fields.
local function eval_consume(expr, s)
    local e = expr:gsub("(%a[%w_]*)", function(name)
        return tostring(math.floor(tonumber(s[name]) or 0))
    end)
    local f = load("return " .. e)
    return f and math.max(0, math.floor(f())) or 0
end

-- extend slot_count to accept self for consume fields
local function slot_count(def, s)
    local opts = def[6] or {}
    if opts.consume then
        return s and eval_consume(opts.consume, s) or 0
    end
    if is_color(def) then return 3 end
    if is_range(def) then return 2 end
    return 1
end

-- Read one logical value from atoms[i..], using type info.
-- Returns nil if the required atoms are absent.
local function atoms_read(def, atoms, i, s)
    local opts = def[6] or {}
    if opts.consume then
        local n = eval_consume(opts.consume, s)
        if not atoms[i] then return nil end
        local t = {}
        for j = 0, n - 1 do
            t[j + 1] = tonumber(atoms[i + j]) or 0
        end
        return t
    end
    if is_color(def) then
        local r = tonumber(atoms[i]); if r == nil then return nil end
        return { r, tonumber(atoms[i+1]) or 0, tonumber(atoms[i+2]) or 0 }
    elseif is_range(def) then
        local lo = tonumber(atoms[i]); if lo == nil then return nil end
        return { lo, tonumber(atoms[i+1]) or 0 }
    else
        return atoms[i]
    end
end

-- Append one logical value to the out list for set_args.
local function atoms_write(def, val, out, s)
    local opts = def[6] or {}
    if opts.consume then
        local n = eval_consume(opts.consume, s)
        for i = 1, n do
            out[#out + 1] = (type(val) == "table" and val[i]) or 0
        end
        return
    end
    if is_color(def) then
        out[#out+1] = math.floor((val[1] or 0) + 0.5)
        out[#out+1] = math.floor((val[2] or 0) + 0.5)
        out[#out+1] = math.floor((val[3] or 0) + 0.5)
    elseif is_range(def) then
        out[#out+1] = val[1]
        out[#out+1] = val[2]
    else
        out[#out+1] = val
    end
end

-- Coerce a raw atom to the correct Lua type for a given def.
local function coerce(def, raw)
    local t = def[5]
    if     t == "int"   then return math.floor(tonumber(raw) or 0)
    elseif t == "float" then return tonumber(raw) or 0
    elseif t == "check" then return (tonumber(raw) or 0) ~= 0 and 1 or 0
    elseif t == "text"  then return tostring(raw or "")
    else                     return raw
    end
end

-- ─── Public API ───────────────────────────────────────────────────────────────

---Install boilerplate methods on `class` based on `defs`.
---@param class table  The pd.Class object (before registration or after).
---@param defs  table  Ordered list of arg definition entries (see file header).
function else_gui.setup(class, defs, flags)
    flags = flags or {}

    -- auto-build single-value flag handlers from opts.flag
    local auto_flags = {}
    for _, def in ipairs(defs) do
        local opts = def[6] or {}
        if opts.flag then
            local ptype = def[5]
            auto_flags["-" .. opts.flag] = function(self, atoms, i)
                if ptype == "color" then
                    self[def[1]] = {
                        tonumber(atoms[i])   or 0,
                        tonumber(atoms[i+1]) or 0,
                        tonumber(atoms[i+2]) or 0,
                    }
                    return 3
                elseif ptype == "range" then
                    self[def[1]] = {
                        tonumber(atoms[i])   or 0,
                        tonumber(atoms[i+1]) or 0,
                    }
                    return 2
                else
                    self[def[1]] = coerce(def, atoms[i])
                    return 1
                end
            end
        end
    end

    -- merge: explicit FLAGS win over auto-generated ones
    local all_flags = {}
    for k, v in pairs(auto_flags) do all_flags[k] = v end
    for k, v in pairs(flags)      do all_flags[k] = v end

    class.init_args = function(self, atoms)
        -- first pass: defaults
        for _, def in ipairs(defs) do
            if type(def[2]) == "function" then
                self[def[1]] = def[2]()
            else
                self[def[1]] = clone(def[2])
            end
        end

        -- second pass: scan atoms, consuming flags then positionals
        local positional = {}
        local i = 1
        while i <= #atoms do
            local a = tostring(atoms[i])
            local handler = all_flags[a]
            if handler then
                i = i + 1
                local consumed = handler(self, atoms, i)
                i = i + consumed
            else
                positional[#positional + 1] = atoms[i]
                i = i + 1
            end
        end

        local slot = 1
        for _, def in ipairs(defs) do
            local raw = atoms_read(def, positional, slot, self)
            if raw ~= nil then self[def[1]] = raw end
            slot = slot + slot_count(def, self)
        end

        if self.key then
            self._key_recv = pd.Receive:new():register(self, "#keyname", "_key_cb")
        end
    end

    class._key_cb = function(self, sel, atoms)
        if self.key then self:key(atoms[1], atoms[2]) end
    end

    -- ── save_args() ───────────────────────────────────────────────────────────
    class.save_args = function(self)
        local out = {}
        for _, def in ipairs(defs) do
            atoms_write(def, self[def[1]], out, self)
        end
        self:set_args(out)
    end

    -- Repaint redirect so that value arguments don't get passed as the optional "layer" argument
    class._repaint = function(self)
        self:repaint()
    end

    class._resize = function(self)
        self:set_size(self.width, self.height)
        self:repaint()
    end

    -- ── properties(p) ─────────────────────────────────────────────────────────
    -- Only installed when the class doesn't already define one.
    if not class.properties then
        class.properties = function(self, p)
            local frames      = {}   -- ordered list of frame names
            local frame_defs  = {}   -- frame_name → { def, ... }
            local DEFAULT     = "Properties"

            -- Pass 1: group defs by frame name
            for i, def in ipairs(defs) do
                if not has_prop(def) then goto continue end
                local opts  = def[6] or {}
                local fname = opts.frame or DEFAULT
                if not frame_defs[fname] then
                    frame_defs[fname] = {}
                    frames[#frames + 1] = fname
                end
                frame_defs[fname][#frame_defs[fname] + 1] = { def=def, pos=i }
                ::continue::
            end

            -- Pass 2: sort each frame bucket, then emit
            for _, fname in ipairs(frames) do
                table.sort(frame_defs[fname], function(a, b)
                    local oa = (a.def[6] or {}).order or math.huge
                    local ob = (b.def[6] or {}).order or math.huge
                    if oa ~= ob then return oa < ob end
                    return a.pos < b.pos   -- stable tiebreaker: original ARGS position
                end)

                -- ncols: first entry in this frame that specifies one
                local ncols = 2
                for _, entry in ipairs(frame_defs[fname]) do
                    if (entry.def[6] or {}).ncols then ncols = entry.def[6].ncols; break end
                end
                p:new_frame(fname, ncols)

                for _, entry in ipairs(frame_defs[fname]) do
                    local def     = entry.def
                    local varname = def[1]
                    local label   = def[4]
                    local ptype   = def[5]
                    local opts    = def[6] or {}
                    local prop_cb = "prop__" .. varname
                    local val     = self[varname]

                    if     ptype == "int"   then
                        p:add_int  (label, prop_cb, val, opts.min or 0, opts.max or 1000)
                    elseif ptype == "float" then
                        p:add_float(label, prop_cb, val, opts.min or -1e9, opts.max or 1e9)
                    elseif ptype == "text"  then
                        p:add_text (label, prop_cb, val)
                    elseif ptype == "check" then
                        p:add_check(label, prop_cb, val)
                    elseif ptype == "color" then
                        p:add_color(label, prop_cb, {val[1], val[2], val[3]})
                    elseif ptype == "combo" then
                        p:add_combo(label, prop_cb, val+1, opts.items or {})
                    elseif ptype == "range" then
                        p:add_float(label .. " Min", prop_cb .. "_start", val[1],
                                    opts.min or -1e9, opts.max or 1e9)
                        p:add_float(label .. " Max", prop_cb .. "_end", val[2],
                                    opts.min or -1e9, opts.max or 1e9)
                    end
                end
            end
        end
    end

    -- ── Per-def: prop__ callback + in_1_ setter ───────────────────────────────
    for _, def in ipairs(defs) do
        local varname = def[1]
        local cb      = def[3]       -- may be nil
        local ptype   = def[5]       -- may be nil
        local opts    = def[6] or {}
        local msgname = opts.msg or varname

        if cb == "repaint" then cb = "_repaint" end
        if cb == "resize" then cb = "_resize" end

        -- prop__<varname> — called by the properties panel after the user edits a value
        if has_prop(def) and is_range(def) then
            class["prop__" .. varname .. "_start"] = function(self, v)
                self[varname] = { v, self[varname][2] }
                self:save_args()
                if cb and self[cb] then self[cb](self, self[varname]) end
            end
            class["prop__" .. varname .. "_end"] = function(self, v)
                self[varname] = { self[varname][1], v }
                self:save_args()
                if cb and self[cb] then self[cb](self, self[varname]) end
            end
        elseif has_prop(def) then
            class["prop__" .. varname] = function(self, v)
                if ptype == "color" then
                    self[varname] = { v[1], v[2], v[3] }
                elseif ptype == "combo" then
                    self[varname] = v - 1
                else
                    self[varname] = v
                end
                self:save_args()
                if cb and self[cb] then self[cb](self, self[varname]) end
            end
        end

        -- in_1_<msg> — auto-generated inlet setter, skipped when:
        --   • no type is known (3-field state-only entries)
        --   • the method already exists (user-defined custom handler)
        if ptype then
            local key = "in_1_" .. msgname
            if not class[key] then
                class[key] = function(self, atoms)
                    local newval
                    if ptype == "color" then
                        newval = {
                            (tonumber(atoms[1]) or 0),
                            (tonumber(atoms[2]) or 0),
                            (tonumber(atoms[3]) or 0),
                        }
                    elseif ptype == "range" then
                        newval = { tonumber(atoms[1]) or 0, tonumber(atoms[2]) or 0 }
                    else
                        newval = coerce(def, atoms[1])
                    end
                    self[varname] = newval
                    self:save_args()
                    if cb and self[cb] then self[cb](self, newval) end
                end
            end
        end
    end
end

return else_gui
