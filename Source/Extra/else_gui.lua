-- else_gui.lua
-- Reusable helpers for Pure Data Lua GUI objects.
--
-- Usage:
--   local else_gui = require("else_gui")
--   else_gui.setup(MyClass, ARGS, EXTRA_FLAGS)
local else_gui = {}

-- A value as text, a number the way Pd shows it: six significant digits,
-- which also hides the rounding of its 32-bit floats (0.3 arrives here as
-- 0.30000001192093).
function else_gui.text(a)
    if type(a) == "number" then return string.format("%g", a) end
    return tostring(a)
end

-- A float method's argument as a number: pdlua hands it a bare number, but
-- a receive_cb passes it on as a table.
function else_gui.as_float(a)
    if type(a) == "table" then return tonumber(a[1]) end
    return tonumber(a)
end

-- The numbers that follow a flag at atoms[i], at most n of them: a flag can
-- be given fewer, or be followed straight away by the next flag.
function else_gui.numbers(atoms, i, n)
    local t = {}
    while #t < n and tonumber(atoms[i + #t]) do t[#t + 1] = tonumber(atoms[i + #t]) end
    return t
end

-- ─── Internal helpers ─────────────────────────────────────────────────────────

local function is_color(def) return def[5] == "color" end
local function is_range(def) return def[5] == "range" end
local function has_prop(def)  return def[4] ~= nil and def[5] ~= nil end

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

-- Number of creation-arg slots consumed by one def entry (consume fields
-- are evaluated against self).
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

-- Keep a number field inside the min/max its def gives, and a combo on one
-- of its items. The properties dialog does that itself, but arguments and
-- messages can be anything, and pdlua's combo box takes the value as an
-- index into its items without checking it.
local function limit(def, v)
    local opts = def[6] or {}
    if type(v) ~= "number" then return v end
    if opts.min and v < opts.min then return opts.min end
    if opts.max and v > opts.max then return opts.max end
    if def[5] == "combo" and opts.items then
        return math.max(0, math.min(#opts.items - 1, v))
    end
    return v
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
        -- Pd saves an empty symbol as nothing at all, which would shift every
        -- argument after it on reload, so save the default ("empty") instead
        if def[5] == "text" and (val == nil or val == "") then val = def[2] end
        -- a name typed with $1, $0... is saved as typed for as long as it
        -- still stands for the same thing (see capture_typed)
        local typed = s and s._typed and s._typed[def[1]]
        if typed and s:realize_symbol(typed) == s:realize_symbol(val) then val = typed end
        out[#out+1] = val
    end
end

-- Coerce a raw atom to the correct Lua type for a given def. A number field
-- given something else (a placeholder such as "holder" or "empty" in an old
-- patch) keeps its default.
local function coerce(def, raw)
    local t = def[5]
    if t == "int" or t == "combo" or t == "float" or t == "check" then
        local n = tonumber(raw)
        if n == nil   then return def[2] end
        if t == "float" then return n end
        if t == "check" then return n ~= 0 and 1 or 0 end
        return math.floor(n)
    elseif t == "text"  then return raw == nil and "" or else_gui.text(raw)
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
                    local v = else_gui.numbers(atoms, i, 3)
                    if #v > 0 then self[def[1]] = { v[1], v[2] or 0, v[3] or 0 } end
                    return #v
                elseif ptype == "range" then
                    local v, cur = else_gui.numbers(atoms, i, 2), self[def[1]]
                    self[def[1]] = { v[1] or cur[1], v[2] or cur[2] }
                    return #v
                elseif ptype == "check" and tonumber(atoms[i]) == nil then
                    -- given bare (last, or followed by another flag): on
                    self[def[1]] = 1
                    return 0
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

    -- ── "$0" handling for symbol fields (opts.dollar) ─────────────────────────
    -- Pd expands "$0" in object box arguments and in message boxes, so a send,
    -- receive or array name often reaches us already expanded ("1003-foo",
    -- "array_1003"). Saving that would bake in an id that is different the
    -- next time the patch is opened, so keep the raw "$0" form in the
    -- arguments and expand it only where the symbol is actually used. The id
    -- is put back where it stands apart from letters and digits, so that a
    -- name like "track1003" is left alone. Names given in the box are kept
    -- as typed anyway (see capture_typed); this is for ones that arrive in
    -- messages.
    class.raw_symbol = function(self, sym)
        if type(sym) ~= "string" then return sym end
        local dz = self:canvas_realizedollar("$0")
        if not dz or dz == "" then return sym end
        return (sym:gsub("%f[%w]" .. dz .. "%f[%W]", "$0"))
    end

    -- a value for field `name` kept inside its def's min/max, for an
    -- object's own message handlers
    local def_of = {}
    for _, def in ipairs(defs) do def_of[def[1]] = def end
    class.limited = function(self, field, v)
        return limit(def_of[field], v)
    end

    -- whether a creation argument is one of this class's flags, for flag
    -- handlers that take a list of words of their own
    class.is_flag = function(self, a)
        return all_flags[tostring(a)] ~= nil
    end

    class.realize_symbol = function(self, sym)
        if type(sym) ~= "string" or sym == "" or sym == "empty" then return sym end
        return self:canvas_realizedollar(sym)
    end

    -- Read creation arguments into t: the flags, then the positional ones,
    -- converted to the declared type, so that a placeholder such as "holder"
    -- in a number slot keeps the default.
    local function parse(t, atoms)
        local positional = {}
        local i = 1
        while i <= #atoms do
            local handler = all_flags[tostring(atoms[i])]
            if handler then
                i = i + 1
                i = i + handler(t, atoms, i)
            else
                positional[#positional + 1] = atoms[i]
                i = i + 1
            end
        end
        local slot = 1
        for _, def in ipairs(defs) do
            local raw = atoms_read(def, positional, slot, t)
            if raw ~= nil then
                t[def[1]] = type(raw) == "table" and raw or coerce(def, raw)
            end
            slot = slot + slot_count(def, t)
        end
        return positional, slot
    end

    class.init_args = function(self, atoms)
        -- defaults first
        for _, def in ipairs(defs) do
            if type(def[2]) == "function" then
                self[def[1]] = def[2]()
            else
                self[def[1]] = clone(def[2])
            end
        end

        local positional, slot = parse(self, atoms)

        -- Limits come last, so that data laid out by these values (a saved
        -- grid) is read as it was saved; what a limit cut is kept in
        -- _unclamped for reading such data.
        for _, def in ipairs(defs) do
            if (def[6] or {}).dollar then
                self[def[1]] = self:raw_symbol(self[def[1]])
            end
            local v = limit(def, self[def[1]])
            if v ~= self[def[1]] then
                self._unclamped = self._unclamped or {}
                self._unclamped[def[1]] = self[def[1]]
            end
            self[def[1]] = v
        end

        if self.key then
            self._key_recv = pd.Receive:new():register(self, "#keyname", "_key_cb")
        end

        -- the positional arguments with the flags taken out, and where the
        -- ones declared here end, for objects that keep more data after them
        return positional, slot
    end

    -- ── receive name ──────────────────────────────────────────────────────────
    -- What arrives at an object's receive name (registered as "receive_cb").
    -- It takes every message the inlet does, except while the object outputs:
    -- its own output coming back in is dropped (the send name may be the
    -- same, or the outlet may lead back to it), though a 'set', which outputs
    -- nothing itself, gets through. [retrieve] (and with it [presets]) sends
    -- "retrieve" to ask for the current state.
    class.receive_cb = function(self, sel, atoms)
        if sel == "retrieve" then return self:retrieve() end
        if self._outputting and sel ~= "set" then return end
        self:dispatch(1, sel, atoms)
    end

    -- The answer to [retrieve] goes out of the outlet only, which [retrieve]
    -- has redirected to itself, and not to the send name, as with the old
    -- abstractions. By default it is what a bang outputs; output() functions
    -- leave out the send while _retrieving is set.
    class.retrieve = function(self)
        self._retrieving = true
        self:in_1_bang()
        self._retrieving = false
    end

    -- Listen at receive name sym ("empty" or "" for none) instead. This can
    -- happen while Pd is delivering a message to the old name (a "receive"
    -- message sent there), and Pd doesn't expect a receiver to go away
    -- then: the others at that name could miss the message, or Pd crash. So
    -- the old receiver is only freed once that is over, and until then
    -- ignores what reaches it.
    class.setup_receive = function(self, sym)
        local old = self.recv
        self.recv = nil
        if old then
            old.dispatch = function() end
            self._old_recvs = self._old_recvs or {}
            self._old_recvs[#self._old_recvs + 1] = old
            self._recv_clock = self._recv_clock
                or pd.Clock:new():register(self, "_free_old_recvs")
            self._recv_clock:delay(0)
        end
        if sym ~= "empty" and sym ~= "" then
            self.recv = pd.Receive:new():register(self, self:realize_symbol(sym), "receive_cb")
        end
    end

    class._free_old_recvs = function(self)
        for _, r in ipairs(self._old_recvs or {}) do r:destruct() end
        self._old_recvs = nil
    end

    class._key_cb = function(self, sel, atoms)
        if self.key then self:key(atoms[1], atoms[2]) end
    end

    -- ── set_args(args) ────────────────────────────────────────────────────────
    -- pdlua runs every string through binbuf_text, which splits it at
    -- whitespace, commas and semicolons. Escape those the way Pd writes them
    -- in a patch file, so that a name like "my send" stays one argument.
    class.set_args = function(self, args)
        local out = {}
        for i, a in ipairs(args) do
            out[i] = type(a) == "string" and (a:gsub("[%s,;\\]", "\\%0")) or a
        end
        pd.Class.set_args(self, out)
    end

    -- The names as typed in the box, with $1, $0... unexpanded: the object
    -- only gets the expanded ones. The box text isn't there yet when the
    -- object is created, so it is read on the first save, before that
    -- overwrites it. A name that still stands for the same thing is then
    -- saved as typed (see atoms_write), so "$1-out" stays "$1-out".
    local function capture_typed(self)
        self._typed = {}
        local args = self:get_args()
        if type(args) ~= "table" then return end
        local atoms = {}
        for i, a in ipairs(args) do   -- get_args gives them escaped
            atoms[i] = type(a) == "string" and (a:gsub("\\(.)", "%1")) or a
        end
        local scratch = setmetatable({}, { __index = self })
        for _, def in ipairs(defs) do scratch[def[1]] = clone(self[def[1]]) end
        if not pcall(parse, scratch, atoms) then return end
        for _, def in ipairs(defs) do
            local typed = rawget(scratch, def[1])
            if (def[6] or {}).dollar and type(typed) == "string" and typed:find("$", 1, true) then
                self._typed[def[1]] = typed
            end
        end
    end

    -- ── save_args() ───────────────────────────────────────────────────────────
    class.save_args = function(self)
        if not self._typed then capture_typed(self) end
        local out = {}
        for _, def in ipairs(defs) do
            atoms_write(def, self[def[1]], out, self)
        end
        -- an object that saves more after these adds it in extra_args(out).
        -- Reading them back with get_args instead would give the names back
        -- escaped, so they would be escaped twice.
        if self.extra_args then self:extra_args(out) end
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
                elseif opts.dollar then
                    self[varname] = self:raw_symbol(v)
                else
                    self[varname] = limit(def, v)
                end
                self:save_args()
                if cb and self[cb] then self[cb](self, self[varname]) end
            end
        end

        -- in_1_<msg> — auto-generated inlet setter, skipped when:
        --   • no type is known (3-field state-only entries)
        --   • the method already exists (user-defined custom handler)
        --   • opts.nomsg is set (the class takes every message itself)
        if ptype and not opts.nomsg then
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
                        newval = limit(def, coerce(def, atoms[1]))
                        if opts.dollar then newval = self:raw_symbol(newval) end
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
