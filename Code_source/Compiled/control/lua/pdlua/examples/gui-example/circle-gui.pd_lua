local circle = pd.Class:new():register("circle-gui")

function circle:initialize(sel, atoms)
    self.slider_position_x = 0.5
    self.slider_position_y = 0.5
    self.inlets = {DATA}
    self.outlets = {DATA}
    self.range_start = -1
    self.range_end = 1
    self.bg = Colors.background
    self.fg = Colors.foreground

    self:set_size(127, 127)
    self:restore_state(atoms)

    return true
end

function circle:restore_state(atoms)
    if #atoms >= 11 then
        local size = atoms[1]
        self.range_start = atoms[2]
        self.range_end = atoms[3]

        self.slider_position_x = atoms[4]
        self.slider_position_y = atoms[5]

        if atoms[7] == -1 then
            self.bg = atoms[6]
        else
            self.bg = {atoms[6], atoms[7], atoms[8]}
        end

        if atoms[10] == -1 then
            self.fg = atoms[9]
        else
            self.fg = {atoms[9], atoms[10], atoms[11]}
        end

        self:set_size(size, size)
        self:repaint()
    end
end

function circle:save_state()
    local width, height = self:get_size()
    local state = {
        width, self.range_start, self.range_end, self.slider_position_x, self.slider_position_y
    }

    local function append_color(color)
        if type(color) == "number" then
            state[#state + 1] = color
            state[#state + 1] = -1
            state[#state + 1] = -1
        else
            state[#state + 1] = color[1]
            state[#state + 1] = color[2]
            state[#state + 1] = color[3]
        end
    end

    append_color(self.bg)
    append_color(self.fg)

    self:set_args(state)
end

function circle:mouse_down(x, y)
    local width, height = self:get_size()
    self.mouse_down_x = x
    self.mouse_down_y = y
    self.mouse_down_slider_x = self.slider_position_x * width
    self.mouse_down_slider_y = self.slider_position_y * height
end

function circle:mouse_drag(x, y)
    local width, height = self:get_size()

    local dx = x - self.mouse_down_x
    local dy = y - self.mouse_down_y

    local new_x = ((self.mouse_down_slider_x + dx) / width) - 0.5
    local new_y = ((self.mouse_down_slider_y + dy) / height) - 0.5

    local distance = math.sqrt(new_x^2 + new_y^2)
    local angle = math.atan(new_y, new_x)

    distance = math.max(0, math.min(0.5, distance))

    new_x = math.cos(angle) * distance + 0.5
    new_y = math.sin(angle) * distance + 0.5

    if new_x ~= self.slider_position_x or new_y ~= self.slider_position_y then
        self.slider_position_x = new_x
        self.slider_position_y = new_y
        self:in_1_bang()
    end

    self:repaint()
end

function circle:mouse_up(x, y)
    self:save_state()
end

function circle:in_1_bang()
    local x = self:scale_value(self.slider_position_x, self.range_start, self.range_end)
    local y = self:scale_value(self.slider_position_y, self.range_start, self.range_end)
    self:outlet(1, "list", {x, y})
end

function circle:in_1_size(atoms)
    self:set_size(atoms[1], atoms[1])
    self:save_state()
end

function circle:in_1_range(atoms)
    self.range_start = atoms[1]
    self.range_end = atoms[2]
    self:save_state()
end

function circle:in_1_bgcolor(atoms)
    self.bg = atoms
    self:repaint()
    self:save_state()
end

function circle:in_1_fgcolor(atoms)
    self.fg = atoms
    self:repaint()
    self:save_state()
end

function circle:scale_value(value, min, max)
    return min + (max - min) * value
end

function circle:paint(g)
    local width, height = self:get_size()
    local x = self.slider_position_x * width
    local y = self.slider_position_y * height

   if type(self.bg) == "number" then
        g:set_color(self.bg)
    elseif type(self.bg) == "table" then
        g:set_color(table.unpack(self.bg))
    end

    g:fill_all()

    if type(self.fg) == "number" then
        g:set_color(self.fg)
    elseif type(self.fg) == "table" then
        g:set_color(table.unpack(self.fg))
    end

    g:fill_ellipse(x - 3.5, y - 3.5, 7, 7)
    g:stroke_ellipse(1, 1, width - 2, height - 2, 1)

    if self.grid ~= 0 then
        g:draw_line(width / 2, 0, width / 2, height, 1)
        g:draw_line(0, height / 2, width, height / 2, 1)
    end

    g:draw_line(x, y, x, height / 2, 2)
    g:draw_line(x, y, width / 2, y, 2)
end
