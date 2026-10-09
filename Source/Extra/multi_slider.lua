-- multi_slider.lua
-- Everything [multi.vsl] and [multi.hsl] share, which is all of it but the
-- direction the sliders go in.
--
-- Usage:
--   local multi_slider = require("multi_slider")
--   multi_slider.setup(MyClass, "multi.vsl", true)   -- true: vertical sliders
local else_gui     = require("else_gui")
local multi_slider = {}

function multi_slider.setup(class, name, vertical)
    else_gui.setup(class, {
        { "width",      200,          "resize",     "Width",      "int",   {frame="Dimensions", min=4,  flag="width"}              },
        { "height",     127,          "resize",     "Height",     "int",   {frame="Dimensions", min=4,  flag="height"}             },
        { "range",      {0, 127},     "on_range",   "Range",      "range", {frame="General",    flag="range"}                      },
        { "n",          8,            "on_n",       "Sliders",    "int",   {frame="Dimensions", min=1,  max=1024, flag="n"}        },
        { "tabname",    "internal",   "on_rename",  "Array Name", "text",  {frame="General",    flag="name", msg="rename", dollar=true}      },
        { "send",       "empty",      nil,          "Send",       "text",  {frame="General",    flag="send", dollar=true}                       },
        { "receive",    "empty",      "on_receive", "Receive",    "text",  {frame="General",    flag="receive", dollar=true}                    },
        { "bgcolor",    pd.bg_color,  "repaint",    "Background", "color", {frame="Appearance", flag="bgcolor"}                    },
        { "fgcolor",    {90, 90, 90}, "repaint",    "Foreground", "color", {frame="Appearance", flag="fgcolor"}                    },
        { "linecolor",  pd.fg_color,  "repaint",    "Line",       "color", {frame="Appearance", flag="linecolor"}                  },
        { "jump",       0,            nil,          "Jump",       "check", {frame="General",    flag="jump"}                       },
        { "savestate",  0,            nil,          "Save State", "check", {frame="General",    flag="savestate"}                  },
        { "mode",       0,            nil,          "List Mode",  "check", {frame="General",    flag="mode"}                       },
    }, {
        -- -dim <w> <h>
        ["-dim"] = function(self, atoms, i)
            self.width  = math.max(4, math.floor(tonumber(atoms[i])   or self.width))
            self.height = math.max(4, math.floor(tonumber(atoms[i+1]) or self.height))
            return 2
        end,
        -- -set <v0> <v1> ...: pre-set slider values. Takes all the numbers
        -- that follow, as the old abstraction did: the number of sliders
        -- isn't known yet when the flags are read (it can come after them),
        -- so the ones past it are left out in initialize.
        ["-set"] = function(self, atoms, i)
            self._preset = {}
            while tonumber(atoms[i + #self._preset]) do
                self._preset[#self._preset + 1] = tonumber(atoms[i + #self._preset])
            end
            return #self._preset
        end,
        -- -savestate is a bare flag here (the help documents it so): a number
        -- after it is the next argument, not its value
        ["-savestate"] = function(self, atoms, i)
            self.savestate = 1
            return 0
        end,
    })

    -- ─── Direction ────────────────────────────────────────────────────────────

    -- Where the pointer is along the row of sliders, and how long the row is.
    local function along(self, x, y)
        if vertical then return x, self.width end
        return y, self.height
    end

    -- The pointer as a 0-1 position on a slider: vertical sliders are at
    -- their top at 1, horizontal ones at their right end.
    local function norm_at(self, x, y)
        if vertical then return 1 - y / self.height end
        return x / self.width
    end

    -- How far the pointer moved along the sliders since the last event, as a
    -- fraction of their length.
    local function moved(self, x, y)
        if vertical then return -(y - self.last_y) / self.height end
        return (x - self.last_x) / self.width
    end

    -- Slider i (counting from 1) runs from edge(i - 1) to edge(i) along a
    -- row len long. The edges are whole pixels: pdlua rounds a rectangle's
    -- position and its size down separately, so fractional ones leave gaps.
    -- Painting and clicking both go by them, so a click hits what is drawn.
    local function edge(self, i, len)
        return math.floor(i * len / self.n + 0.5)
    end

    function class:idx_at(x, y)
        local pos, len = along(self, x, y)
        for i = 1, self.n - 1 do
            if pos < edge(self, i, len) then return i end
        end
        return self.n
    end

    function class:val_at(x, y)
        return self:clamp_val(self:norm_to_val(norm_at(self, x, y)))
    end

    -- ─── Saved state ──────────────────────────────────────────────────────────

    -- in save state mode the values are saved after the declared arguments
    function class:extra_args(out)
        if self.savestate ~= 0 and self.values then
            for i = 1, self.n do out[#out + 1] = self.values[i] or 0 end
        end
    end

    -- Mirror the values into the array, if there is one, and save. only: the
    -- one slider that changed, when that is all (a drag), so the rest of the
    -- array is left as it is. This comes before the values go out, so that
    -- what reads the array then gets them, and what reads it back into us
    -- (a bang) doesn't undo the change.
    function class:save_state(only)
        local t = self:table()
        if t then
            for i = only or 1, only or self.n do t:set(i - 1, self.values[i] or 0) end
            t:redraw()
        end
        self:save_args()
    end

    -- ─── Messages ─────────────────────────────────────────────────────────────

    function class:in_1_list(atoms)
        self:refresh_from_table()
        for i, a in ipairs(atoms) do
            local idx = i   -- 1-based internally
            if idx <= self.n then
                self.values[idx] = self:clamp_val(tonumber(a) or 0)
            end
        end
        self:save_state()
        self:on_change_all(); self:repaint()
    end

    -- a number is a list of one: it sets the first slider
    function class:in_1_float(f)
        self:in_1_list({ f })
    end

    -- set <index> <v0> <v1> ...: set values starting from 1-based index
    function class:in_1_set(atoms)
        self:refresh_from_table()
        local start = math.floor(tonumber(atoms[1]) or 0) + 1  -- convert to 1-based
        for i = 2, #atoms do
            local idx = start + (i - 2)
            if idx >= 1 and idx <= self.n then
                self.values[idx] = self:clamp_val(tonumber(atoms[i]) or 0)
            end
        end
        self:save_state(); self:repaint()
    end

    -- get <index> ...: output "index value" for each slider asked for
    function class:in_1_get(atoms)
        self:refresh_from_table()
        for _, a in ipairs(atoms) do
            local idx = math.floor(tonumber(a) or -1) + 1
            if idx >= 1 and idx <= self.n then
                self:out("list", { idx - 1, self.values[idx] })
            end
        end
    end

    -- dim <w> <h>: set width and height together
    function class:in_1_dim(atoms)
        if atoms[1] then self.width  = math.max(4, math.floor(tonumber(atoms[1]) or self.width))  end
        if atoms[2] then self.height = math.max(4, math.floor(tonumber(atoms[2]) or self.height)) end
        self:set_size(self.width, self.height)
        self:save_args(); self:repaint()
    end

    -- range <lo> <hi>
    function class:in_1_range(atoms)
        self:refresh_from_table()
        local old_lo, old_hi = self.range[1], self.range[2]
        local new_lo = tonumber(atoms[1]) or old_lo
        local new_hi = tonumber(atoms[2]) or old_hi
        local old_span = old_hi - old_lo
        local new_span = new_hi - new_lo

        for i = 1, self.n do
            if old_span ~= 0 then
                local norm = (self.values[i] - old_lo) / old_span
                self.values[i] = new_lo + norm * new_span
            else
                self.values[i] = new_lo
            end
        end

        local t = self:table()
        if t then
            for i = 1, self.n do t:set(i - 1, self.values[i] or 0) end
            t:redraw()
        end

        self.range = { new_lo, new_hi }
        self:clamp_all()
        self:save_args(); self:repaint()
    end

    -- import <v0> <v1> ...: the number of values sets the number of sliders
    function class:in_1_import(atoms)
        local vals = {}
        for _, a in ipairs(atoms) do vals[#vals + 1] = tonumber(a) or 0 end
        if #vals == 0 then return end
        self:resize_sliders(#vals)
        for i, v in ipairs(vals) do self.values[i] = self:clamp_val(v) end
        self:save_state()
        self:on_change_all(); self:repaint()
    end

    function class:in_1_bang()
        self:refresh_from_table()
        if self.mode ~= 0 then
            self:output_all_list()
        else
            self:in_1_dump()
        end
    end

    function class:in_1_dump()
        self:refresh_from_table()
        for i = 1, self.n do
            self:out("list", { i - 1, self.values[i] })
        end
    end

    function class:in_1_setall(atoms)
        local v = self:clamp_val(tonumber(atoms[1]) or 0)
        for i = 1, self.n do self.values[i] = v end
        self:save_state(); self:repaint()
    end

    -- the dump goes out with an "export" selector so it can be told apart from
    -- ordinary slider output with [route export]
    function class:in_1_export()
        self:refresh_from_table()
        local out = {}
        for i = 1, self.n do out[i] = self.values[i] end
        self:out("export", out)
    end

    -- ─── Lifecycle ────────────────────────────────────────────────────────────

    function class:initialize(sel, atoms)
        self.inlets    = 1
        self.outlets   = 1
        self.values    = {}
        self.dragging  = false
        self.drag_idx  = nil
        self.last_x    = nil
        self.last_y    = nil
        self.shift     = false
        self.alt       = false

        local positional, first = self:init_args(atoms)
        -- values of our own come from save state or -set (see table)
        self._push = self.savestate ~= 0 or self._preset ~= nil

        -- allocate values
        self:resize_sliders(self.n)

        -- restore embedded savestate values: they follow the declared arguments.
        -- Patches saved by the old abstraction have "holder" placeholders
        -- between those and the values.
        if self.savestate ~= 0 then
            while positional[first] ~= nil and tonumber(positional[first]) == nil do
                first = first + 1
            end
            for i = 1, self.n do
                local raw = tonumber(positional[first + i - 1])
                if raw then self.values[i] = self:clamp_val(raw) end
            end
        end

        -- apply the -set values
        if self._preset then
            for i = 1, math.min(self.n, #self._preset) do
                self.values[i] = self:clamp_val(self._preset[i])
            end
            self._preset = nil
        end

        -- sync from external table if named
        self:refresh_from_table()

        self.load_clock = pd.Clock:new():register(self, "after_load")
        self.load_clock:delay(0)

        self:setup_receive(self.receive)
        self:set_size(self.width, self.height)
        return true
    end

    -- Once the patch has loaded: an array that comes later in the patch is
    -- there now, so it gets our values if it should (see table), or we show
    -- its own. One that still doesn't exist is reported, rather than
    -- leaving what reads it silently empty.
    function class:after_load()
        if self:has_array() then
            if self:table() then
                self:sync_from_table(); self:repaint()
            else
                self:error(name .. ": no such array '" ..
                           tostring(self:realize_symbol(self.tabname)) .. "'")
            end
        end
        -- only the array we load with gets our values: one named later
        -- has values of its own
        self._push = nil
    end

    function class:on_n()
        self:refresh_from_table()
        self:resize_sliders(self.n)
        self:save_state(); self:repaint()
    end

    function class:on_rename()
        if self:has_array() then
            if self:table() then
                self:sync_from_table()
            else
                self:error(name .. ": no such array '" ..
                           tostring(self:realize_symbol(self.tabname)) .. "'")
            end
        end
        self:repaint()
    end

    function class:on_receive(sym)
        self:setup_receive(sym)
    end

    -- a new range from the properties: keep the values inside it
    function class:on_range()
        self:clamp_all()
        self:save_state(); self:repaint()
    end

    -- ─── Values ───────────────────────────────────────────────────────────────

    function class:clamp_val(v)
        local lo = math.min(self.range[1], self.range[2])
        local hi = math.max(self.range[1], self.range[2])
        return math.max(lo, math.min(hi, v))
    end

    function class:clamp_all()
        for i = 1, self.n do
            self.values[i] = self:clamp_val(self.values[i] or 0)
        end
    end

    function class:resize_sliders(new_n)
        local old = self.values or {}
        self.values = {}
        for i = 1, new_n do
            self.values[i] = self:clamp_val(old[i] or self.range[1])
        end
        self.n = new_n
    end

    function class:val_to_norm(v)
        local lo, hi = self.range[1], self.range[2]
        if lo == hi then return 0 end
        return (v - lo) / (hi - lo)
    end

    function class:norm_to_val(n)
        return self.range[1] + n * (self.range[2] - self.range[1])
    end

    -- ─── Output ───────────────────────────────────────────────────────────────

    -- Everything goes out of the outlet and to the send name alike. What we
    -- output can come straight back in through our receive name (it is the
    -- same as the send name, or the outlet or another object sends it there),
    -- so while we do, receive_cb drops what would output again.
    function class:out(sel, atoms)
        local outer = self._outputting
        self._outputting = true
        self:outlet(1, sel, atoms)
        if self.send ~= "empty" and self.send ~= "" then
            pd.send(self:realize_symbol(self.send), sel, atoms)
        end
        self._outputting = outer   -- outputs can nest, so restore
    end

    function class:output_single(idx)
        if self.mode ~= 0 then
            -- list mode: output all values as a list
            self:output_all_list()
        else
            self:out("list", { idx - 1, self.values[idx] })
        end
    end

    function class:output_all_list()
        local out = {}
        for i = 1, self.n do out[i] = self.values[i] end
        self:out("list", out)
    end

    function class:on_change_all()
        if self.mode ~= 0 then
            self:output_all_list()
        else
            for i = 1, self.n do
                self:out("list", { i - 1, self.values[i] })
            end
        end
    end

    function class:finalize()
        self.load_clock:destruct()
        if self.recv then self.recv:destruct() end
    end

    -- [retrieve] (and so [presets]) gets all the values as one list, whatever
    -- the mode, as the old abstraction gave them; a bang in index mode would
    -- output one message per slider. See receive_cb in else_gui.
    function class:retrieve()
        self:refresh_from_table()
        local out = {}
        for i = 1, self.n do out[i] = self.values[i] end
        self:outlet(1, "list", out)
    end

    -- ─── Array ────────────────────────────────────────────────────────────────

    -- Whether we mirror the values into an array. "internal" keeps them
    -- internally, and so does "empty": patches saved by the old abstraction
    -- have that for no name.
    function class:has_array()
        local n = self.tabname
        return n ~= "internal" and n ~= "empty" and n ~= ""
    end

    -- The array we mirror the values into, or nil when we keep them internally.
    -- The first time it's there, an array gets the values restored from our
    -- arguments (save state, -set), instead of its values replacing them: a
    -- [table] doesn't save its contents, so it reopens empty.
    function class:table()
        if not self:has_array() then return nil end
        local t = pd.Table:new():sync(self:realize_symbol(self.tabname))
        if t and self._push then
            self._push = nil
            for i = 1, math.min(self.n, t:length()) do t:set(i - 1, self.values[i] or 0) end
            t:redraw()
        end
        return t
    end

    -- Another object may have written to the array, so re-read it before the
    -- values are output or changed: changing several of them writes them all
    -- back, which would undo what was written.
    function class:refresh_from_table()
        if self:has_array() then
            self:sync_from_table()
        end
    end

    function class:sync_from_table()
        local t = self:table()
        if not t then return end
        local len = math.min(self.n, t:length())
        for i = 1, len do
            self.values[i] = self:clamp_val(t:get(i - 1))
        end
    end

    -- ─── Mouse and keys ───────────────────────────────────────────────────────

    function class:mouse_down(x, y)
        self:refresh_from_table()
        self.dragging = true
        self.drag_idx = self:idx_at(x, y)
        self.last_x, self.last_y = x, y

        if self.jump ~= 0 then
            self.values[self.drag_idx] = self:val_at(x, y)
            self:save_state(self.drag_idx)
            self:output_single(self.drag_idx); self:repaint()
        end
    end

    function class:mouse_drag(x, y)
        if not self.dragging then return end

        if self.jump ~= 0 then
            -- jump on click: draw across the sliders you pass over, setting each
            -- one to the pointer. Holding alt/option keeps you on the slider you
            -- started from.
            if not self.alt then self.drag_idx = self:idx_at(x, y) end
            self.values[self.drag_idx] = self:val_at(x, y)
        else
            -- steady on click: relative drag on the slider you pressed on,
            -- with shift for fine tuning
            local scale = self.shift and 10 or 1
            local span  = self.range[2] - self.range[1]
            local delta = moved(self, x, y) * span / scale
            self.values[self.drag_idx] = self:clamp_val(
                (self.values[self.drag_idx] or 0) + delta)
        end
        self.last_x, self.last_y = x, y

        self:save_state(self.drag_idx)
        self:output_single(self.drag_idx); self:repaint()
    end

    function class:mouse_up()
        self.dragging = false
    end

    function class:key(down, key)
        if key == "Shift_L" or key == "Shift_R" then
            self.shift = (down ~= 0)
        elseif key == "Alt_L" or key == "Alt_R" then
            self.alt = (down ~= 0)
        end
    end

    -- ─── Paint ────────────────────────────────────────────────────────────────

    function class:paint(g)
        local w, h  = self:get_size()
        local bg    = self.bgcolor
        local fg    = self.fgcolor
        local lc    = self.linecolor

        -- background
        g:set_color(bg[1], bg[2], bg[3])
        g:fill_all()

        -- slider fills: up from the bottom, or right from the left edge. The
        -- bars end on whole pixels too, see edge() above.
        g:set_color(fg[1], fg[2], fg[3])
        for i = 1, self.n do
            local norm = self:val_to_norm(self.values[i])
            if vertical then
                local x0, x1 = edge(self, i - 1, w), edge(self, i, w)
                local top = math.floor(h - norm * h + 0.5)
                g:fill_rect(x0, top, x1 - x0, h - top)
            else
                local y0, y1 = edge(self, i - 1, h), edge(self, i, h)
                g:fill_rect(0, y0, math.floor(norm * w + 0.5), y1 - y0)
            end
        end

        -- a value line at the end of each bar, and dividers between them
        g:set_color(lc[1], lc[2], lc[3])
        for i = 1, self.n do
            local norm = self:val_to_norm(self.values[i])
            if vertical then
                local x0, x1 = edge(self, i - 1, w), edge(self, i, w)
                local top = math.floor(h - norm * h + 0.5)
                g:draw_line(x0, top, x1, top, 1.5)
                if i > 1 then g:draw_line(x0, 0, x0, h, 1) end
            else
                local y0, y1 = edge(self, i - 1, h), edge(self, i, h)
                local bar = math.floor(norm * w + 0.5)
                g:draw_line(bar, y0, bar, y1, 1.5)
                if i > 1 then g:draw_line(0, y0, w, y0, 1) end
            end
        end

        -- for plugdata dynamic resize
        if w ~= self.width or h ~= self.height then
            self.width = w
            self.height = h
            self:save_args()
        end
    end
end

return multi_slider
