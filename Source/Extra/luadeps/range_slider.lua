-- range_slider.lua
-- Everything [range.hsl] and [range.vsl] share, which is all of it but the
-- direction the range goes in.
--
-- Usage:
--   local range_slider = require("range_slider")
--   range_slider.setup(MyClass, "range.hsl", false)   -- false: horizontal
--   range_slider.setup(MyClass, "range.vsl", true)    -- true: vertical
local else_gui     = require("else_gui")
local range_slider = {}

-- Parse a hex color string that MUST start with '#'.
-- Accepts "#RRGGBB" or "#RGB"; returns {r, g, b}, or nil if invalid.
local function hex_to_rgb(hex)
    if type(hex) ~= "string" then return nil end
    if hex:sub(1, 1) ~= "#" then return nil end
    hex = hex:sub(2)
    if #hex == 3 then hex = hex:gsub("(.)", "%1%1") end
    if #hex ~= 6 then return nil end
    local r = tonumber(hex:sub(1, 2), 16)
    local g = tonumber(hex:sub(3, 4), 16)
    local b = tonumber(hex:sub(5, 6), 16)
    if not (r and g and b) then return nil end
    return { r, g, b }
end

local function color_from_atoms(atoms)
    if #atoms == 1 and type(atoms[1]) == "string" then
        return hex_to_rgb(atoms[1])
    end
    if #atoms == 3
       and type(atoms[1]) == "number"
       and type(atoms[2]) == "number"
       and type(atoms[3]) == "number" then
        local r, g, b = atoms[1], atoms[2], atoms[3]
        if r >= 0 and r <= 255
           and g >= 0 and g <= 255
           and b >= 0 and b <= 255 then
            return { r, g, b }
        end
    end
    return nil
end

local function sanitize_color(c)
    if type(c) == "table"
       and type(c[1]) == "number"
       and type(c[2]) == "number"
       and type(c[3]) == "number"
       and c[1] >= 0 and c[1] <= 255
       and c[2] >= 0 and c[2] <= 255
       and c[3] >= 0 and c[3] <= 255 then
        return { c[1], c[2], c[3] }
    end
    return { 0, 0, 0 }
end

function range_slider.setup(class, name, vertical)
    else_gui.setup(class, {
        { "width",     127,           "resize",     "Width",      "int",   {frame="Dimensions", min=10, max=4096, flag="width"}   },
        { "height",    20,            "resize",     "Height",     "int",   {frame="Dimensions", min=4,  max=256,  flag="height"}  },
        { "range",     {0, 127},      "on_range",   "Range",      "range", {frame="General",    flag="range"}                     },
        { "mode",      0,             nil,          "Base Mode",  "combo", {frame="General",    flag="mode", items={"Move","Select","Extend","Expand"}}},
        { "bgcolor",   pd.bg_color,   "repaint",    "Background", "color", {frame="Appearance", flag="bgcolor", theme="bg"}       },
        { "fgcolor",   pd.fg_color,   "repaint",    "Range Fill", "color", {frame="Appearance", flag="fgcolor", theme="fg"}       },
        { "send",      "empty",        nil,         "Send",       "text",  {frame="General",    flag="send", dollar=true}                      },
        { "receive",   "empty",       "on_receive", "Receive",    "text",  {frame="General",    flag="receive", dollar=true}                   },
        { "savestate", 0,             "save_state", "Save State", "check", {frame="General",    flag="savestate"}                 },
        { "init_lo",   0,             nil,          nil,          "float"                                                        },
        { "init_hi",   0,             nil,          nil,          "float"                                                        },
        { "usetheme",    1, "repaint", "Use Theme Colors", "check", {frame="Appearance", order=0, flag="usetheme"} },
        { "transparent", 0, "repaint", "Transparent",      "check", {frame="Appearance", order=0, flag="transparent"} },
    }, {
        ["-range"] = function(self, atoms, i)
            local v = else_gui.numbers(atoms, i, 2)
            self.range = { v[1] or self.range[1], v[2] or self.range[2] }
            self:clamp_values()
            return #v
        end,
        ["-dim"] = function(self, atoms, i)
            local v = else_gui.numbers(atoms, i, 2)
            self.width, self.height = v[1] or self.width, v[2] or self.height
            return #v
        end,
        ["-size"] = function(self, atoms, i)
            local v = else_gui.numbers(atoms, i, 1)
            self.width, self.height = v[1] or self.width, v[1] or self.height
            return #v
        end,
        ["-transparent"] = function(self, atoms, i)
            local v = else_gui.numbers(atoms, i, 1)
            self.transparent = (v[1] == nil or v[1] ~= 0) and 1 or 0
            return #v
        end,
        ["-theme"] = function(self, atoms, i)
            self.usetheme = 1
            return 0
        end,
        ["-savestate"] = function(self, atoms, i)
            self.savestate = 1
            return 0
        end,
        ["-bgcolor"] = function(self, atoms, i)
            local c = hex_to_rgb(atoms[i])
            if c then self.bgcolor = c; self.usetheme = 0 end
            return 1
        end,
        ["-fgcolor"] = function(self, atoms, i)
            local c = hex_to_rgb(atoms[i])
            if c then self.fgcolor = c; self.usetheme = 0 end
            return 1
        end,
    })

    -- ─── Axis helpers ─────────────────────────────────────────────────────────

    -- value -> pixel along the bar. Horizontal: left is low.
    -- Vertical: bottom is low (matches multi_slider convention).
    local function val_to_px(self, v)
        local w, h = self:get_size()
        local len  = vertical and h or w
        local span = self.range[2] - self.range[1]
        if span == 0 then return 0 end
        local t = (v - self.range[1]) / span
        if vertical then return (1 - t) * len end
        return t * len
    end

    -- pixel -> value along the bar
    local function px_to_val(self, x, y)
        local w, h = self:get_size()
        local len  = vertical and h or w
        local pos  = vertical and y or x
        local t    = (len == 0) and 0 or (pos / len)
        if vertical then t = 1 - t end
        local v = self.range[1] + t * (self.range[2] - self.range[1])
        local lo, hi = self:bounds()
        return math.max(lo, math.min(hi, v))
    end

    -- the pointer's pixel coordinate along the bar (for Move's delta)
    local function along(self, x, y)
        return vertical and y or x
    end

    function class:val_to_px(v) return val_to_px(self, v) end
    function class:px_to_val(x, y) return px_to_val(self, x, y) end

    -- ─── Lifecycle ────────────────────────────────────────────────────────────

    function class:initialize(sel, atoms)
        self.inlets   = 1
        self.outlets  = 1

        self.key_shift = false
        self.key_ctrl  = false
        self.key_alt   = false
        self.dragging  = false

        self:init_args(atoms)

        self.bgcolor = sanitize_color(self.bgcolor)
        self.fgcolor = sanitize_color(self.fgcolor)

        if self.savestate ~= 0 then
            self.val_lo, self.val_hi = self.init_lo, self.init_hi
            self:clamp_values()
        else
            local span = self.range[2] - self.range[1]
            self.val_lo = self.range[1] + span / 3
            self.val_hi = self.range[1] + 2 * span / 3
        end

        self:setup_receive(self.receive)
        self:set_size(self.width, self.height)
        return true
    end

    function class:save_state()
        if self.savestate ~= 0 then
            self.init_lo, self.init_hi = self.val_lo, self.val_hi
        end
        self:save_args()
    end

    function class:finalize()
        if self.recv then self.recv:destruct() end
    end

    -- ─── Inlets ───────────────────────────────────────────────────────────────

    function class:in_1_range(atoms)
        self.range = {
            tonumber(atoms[1]) or self.range[1],
            tonumber(atoms[2]) or self.range[2],
        }
        self:clamp_values()
        self:save_state(); self:repaint()
    end

    function class:in_1_list(atoms)
        self.val_lo = tonumber(atoms[1]) or self.val_lo
        self.val_hi = tonumber(atoms[2]) or self.val_hi
        self:clamp_values()
        self:save_state(); self:repaint(); self:output()
    end

    function class:in_1_set(atoms)
        self.val_lo = tonumber(atoms[1]) or self.val_lo
        self.val_hi = tonumber(atoms[2]) or self.val_hi
        self:clamp_values()
        self:save_state(); self:repaint()
    end

    function class:in_1_theme(atoms)
        local v = tonumber(atoms[1])
        if v == nil then return end
        self.usetheme = v ~= 0 and 1 or 0
        self:save_state(); self:repaint()
    end

    function class:in_1_bgcolor(atoms)
        local c = color_from_atoms(atoms)
        if not c then return end
        self.bgcolor = c
        self.usetheme = 0
        self:save_state(); self:repaint()
    end

    function class:in_1_colors(atoms)
        if #atoms ~= 2 then return end
        if type(atoms[1]) ~= "string" or type(atoms[2]) ~= "string" then return end
        local c1 = hex_to_rgb(atoms[1])
        local c2 = hex_to_rgb(atoms[2])
        if not c1 or not c2 then return end
        self.bgcolor = c1
        self.fgcolor = c2
        self.usetheme = 0
        self:save_state(); self:repaint()
    end

    function class:in_1_fgcolor(atoms)
        local c = color_from_atoms(atoms)
        if not c then return end
        self.fgcolor = c
        self.usetheme = 0
        self:save_state(); self:repaint()
    end

    function class:in_1_bang()
        self:output()
    end

    function class:on_receive(sym)
        self:setup_receive(sym)
    end

    function class:on_range()
        self:clamp_values()
        self:save_state(); self:repaint()
    end

    -- ─── Values ───────────────────────────────────────────────────────────────

    function class:bounds()
        return math.min(self.range[1], self.range[2]), math.max(self.range[1], self.range[2])
    end

    function class:clamp_values()
        local lo, hi = self:bounds()
        self.val_lo = math.max(lo, math.min(hi, self.val_lo))
        self.val_hi = math.max(lo, math.min(hi, self.val_hi))
        if self.val_lo > self.val_hi then
            self.val_lo, self.val_hi = self.val_hi, self.val_lo
        end
    end

    function class:output()
        local outer = self._outputting
        self._outputting = true
        self:outlet(1, "list", { self.val_lo, self.val_hi })
        if self.send ~= "empty" and self.send ~= "" and not self._retrieving then
            pd.send(self:realize_symbol(self.send), "list", { self.val_lo, self.val_hi })
        end
        self._outputting = outer
    end

    -- ─── Keys ─────────────────────────────────────────────────────────────────

    function class:active_mode()
        if self.key_shift then return 1 end
        if self.key_ctrl  then return 2 end
        if self.key_alt   then return 3 end
        return self.mode
    end

    function class:key(down, keyname)
        local held = (down ~= 0)
        if     keyname == "Shift_L"   or keyname == "Shift_R"               then self.key_shift = held
        elseif keyname == "Control_L" or keyname == "Control_R"
            or keyname == "Meta_L"    or keyname == "Meta_R"                 then self.key_ctrl  = held
        elseif keyname == "Alt_L"     or keyname == "Alt_R"                  then self.key_alt   = held
        end
    end

    -- ─── Mouse ────────────────────────────────────────────────────────────────

    function class:mouse_down(x, y)
        self.dragging = true
        self.drag_mode = self:active_mode()
        local m = self.drag_mode
        local v = px_to_val(self, x, y)

        if m == 0 then
            -- Move: record anchor in value space
            self.drag_down_axis = along(self, x, y)
            self.drag_down_lo   = self.val_lo
            self.drag_down_hi   = self.val_hi

        elseif m == 1 then
            -- Select: fresh selection from the click point
            self.drag_anchor = v
            self.val_lo, self.val_hi = v, v
            self:repaint(); self:output()

        elseif m == 2 then
            -- Extend: nearest endpoint snaps to the click right away
            self.drag_endpoint =
                math.abs(v - self.val_lo) <= math.abs(v - self.val_hi) and "lo" or "hi"
            if self.drag_endpoint == "lo" then
                self.val_lo = math.min(v, self.val_hi)
            else
                self.val_hi = math.max(v, self.val_lo)
            end
            self:clamp_values()
            self:repaint(); self:output()

        elseif m == 3 then
            -- Expand: the scale axis is perpendicular to the bar.
            -- Vertical bar -> use x. Horizontal bar -> use y.
            self.drag_down_axis   = vertical and x or y
            self.drag_down_center = (self.val_lo + self.val_hi) / 2
            self.drag_down_half   = (self.val_hi - self.val_lo) / 2
        end
    end

    function class:mouse_drag(x, y)
        if not self.dragging then return end
        local m = self.drag_mode
        local w, h = self:get_size()
        local span_val = self.range[2] - self.range[1]

        if m == 0 then
            -- Move: translate both endpoints by the delta in value space
            local len    = vertical and h or w
            local dx_px  = along(self, x, y) - self.drag_down_axis
            local dx_val = (len == 0) and 0 or (dx_px / len) * span_val
            if vertical then dx_val = -dx_val end
            local rng_w  = self.drag_down_hi - self.drag_down_lo
            local lo, hi = self:bounds()
            local new_lo = math.max(lo, math.min(hi - rng_w, self.drag_down_lo + dx_val))
            self.val_lo = new_lo
            self.val_hi = new_lo + rng_w

        elseif m == 1 then
            local v = px_to_val(self, x, y)
            self.val_lo = math.min(self.drag_anchor, v)
            self.val_hi = math.max(self.drag_anchor, v)

        elseif m == 2 then
            local v = px_to_val(self, x, y)
            if self.drag_endpoint == "lo" then
                self.val_lo = math.min(v, self.val_hi)
            else
                self.val_hi = math.max(v, self.val_lo)
            end

        elseif m == 3 then
            -- Expand: drag away from the bar = wider, toward it = narrower.
            -- Vertical bar: x grows right (positive = wider).
            -- Horizontal bar: y grows down (negative = wider, so flip).
            local now_axis = vertical and x or y
            local dy       = (now_axis - self.drag_down_axis) / 8
            if not vertical then dy = -dy end
            local scale    = math.abs(span_val) / (vertical and w or h)
            local new_half = math.max(0, self.drag_down_half + dy * scale)
            local center   = self.drag_down_center
            local lo, hi   = self:bounds()
            self.val_lo    = math.max(lo, center - new_half)
            self.val_hi    = math.min(hi, center + new_half)
        end

        self:clamp_values()
        self:repaint(); self:output()
    end

    function class:mouse_up()
        self.dragging = false
        self:save_state()
    end

    -- ─── Paint ────────────────────────────────────────────────────────────────

    function class:paint(g)
        local w, h = self:get_size()
        local fg   = self:color("fgcolor")

        -- low/high in pixels along the axis
        local p_lo = val_to_px(self, self.val_lo)
        local p_hi = val_to_px(self, self.val_hi)
        local a_lo = math.min(p_lo, p_hi)
        local a_hi = math.max(p_lo, p_hi)

        -- background
        self:fill_background(g, "bgcolor")

        -- range fill
        g:set_color(fg[1], fg[2], fg[3])
        if vertical then
            -- y grows down; a_lo is the smaller y (higher on screen)
            g:fill_rect(0, a_lo, w + 1, math.max(1, a_hi - a_lo))
        else
            g:fill_rect(a_lo, 0, math.max(1, a_hi - a_lo), h + 1)
        end

        -- endpoint handles
        g:set_color(fg[1] * 0.7, fg[2] * 0.7, fg[3] * 0.7)
        if vertical then
            g:fill_rect(0, a_lo, w + 1, 2)
            g:fill_rect(0, a_hi - 2, w + 1, 2)
        else
            g:fill_rect(a_lo, 0, 2, h + 1)
            g:fill_rect(a_hi - 2, 0, 2, h + 1)
        end

        if w ~= self.width or h ~= self.height then
            self.width = w
            self.height = h
            self:save_args()
        end
    end
end

return range_slider