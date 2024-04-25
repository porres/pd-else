local circle = pd.Class:new():register("circle-gui")

function math.clamp(x, min, max)
    if x < min then return min end
    if x > max then return max end
    return x
end

function circle:initialize(sel, atoms)
    self.slider_position_x = 0.5
    self.slider_position_y = 0.5
    self.inlets = 1
    self.outlets = 1
    self.gui = 1
    self.x_range_start = -1
    self.x_range_end = 1
    self.y_range_end = -1
    self.y_range_start = 1
    self.clip = 1
    self.mode = 1
    self.savestate = 0
    self.grid = 1
    self.shift = 0
    self.bg1 = Colors.background
    self.bg2 = Colors.background
    self.fg = Colors.foreground
    self.jump = 0
    self.send_sym = "empty"
    self.receive_sym = "empty"

    self:set_size(127, 127)
    return true
end

function circle:mouse_down(x, y)
    local width, height = self:get_size()

    if self.jump == 1 then
        self.slider_position_x = (x / width)
        self.slider_position_y = 1.0 - (y / height)
        self:in_1_bang()
    end

    self.mouse_down_x = x
    self.mouse_down_y = y
    self.last_mouse_x = x
    self.last_mouse_y = y
    self.mouse_down_slider_x = self.slider_position_x * width
    self.mouse_down_slider_y = (1.0 - self.slider_position_y) * height
    self:repaint()
end

function circle:mouse_drag(x, y)
    local width, height = self:get_size()
    local new_x, new_y;

    local dx = x - self.mouse_down_x
    local dy = y - self.mouse_down_y

    if self.shift ~= 0 then
        dx = dx / 10
        dy = dy / 10
    end

    new_x = ((self.mouse_down_slider_x + dx) / width) - 0.5
    new_y = ((self.mouse_down_slider_y + dy) / height) - 0.5

    if self.clip ~= 0 then
        local distance = math.sqrt(new_x^2 + new_y^2)
        local angle = math.atan(new_y, new_x)

        distance = math.clamp(distance, 0.0, 0.5)

        self.slider_position_x = (math.cos(angle) * distance) + 0.5
        self.slider_position_y = -(math.sin(angle) * distance) + 0.5
    else
        self.slider_position_x = math.clamp(new_x + 0.5, 0, 1)
        self.slider_position_y = -math.clamp(new_y + 0.5, 0, 1)
    end

    self:in_1_bang()
    self:repaint()

    self.last_mouse_x = x
    self.last_mouse_y = y
end

function circle:mouse_up(x, y)
    self:save_state()
end

function circle:restore_state(atoms)
    local index = 2
    if #atoms >= 22 then
        local size = atoms[index]
        self.x_range_start = atoms[index + 1]
        self.x_range_end = atoms[index + 2]
        self.y_range_end = atoms[index + 3]
        self.y_range_start = atoms[index + 4]
        self.mode = atoms[index + 5]

        self.bg1 = {atoms[index + 6], atoms[index + 7], atoms[index + 8]}
        self.bg2 = {atoms[index + 9], atoms[index + 10], atoms[index + 11]}
        self.fg = {atoms[index + 12], atoms[index + 13], atoms[index + 14]}

        if self.bg1[2] == -1 then
            self.bg1 = self.bg1[1]
        end
        if self.bg2[2] == -1 then
            self.bg2 = self.bg2[1]
        end
        if self.fg[2] == -1 then
            self.fg = self.fg[1]
        end

        self.grid = atoms[index + 15]
        self.jump = atoms[index + 16]
        self.savestate = atoms[index + 17]
        if self.savestate ~= 0 then
            self.slider_position_x = self:unscale_value(atoms[index + 18], self.x_range_start, self.x_range_end)
            self.slider_position_y = self:unscale_value(atoms[index + 19], self.y_range_end, self.y_range_start)
        end
        self.send_sym = atoms[index + 20]
        self.receive_sym = atoms[index + 21]
        self.clip = atoms[index + 22]
        self:set_size(size, size)
        self:repaint()
    end
end


function circle:save_state()
    local width, height = self:get_size()

    local state = {
        "state", width, self.x_range_start, self.x_range_end, self.y_range_end, self.y_range_start, self.mode
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

    append_color(self.bg1)
    append_color(self.bg2)
    append_color(self.fg)

    -- Add the rest of the values
    state[#state + 1] = self.grid
    state[#state + 1] = self.jump
    state[#state + 1] = self.savestate
    state[#state + 1] = self:scale_value(self.slider_position_x, self.x_range_start, self.x_range_end)
    state[#state + 1] = self:scale_value(self.slider_position_y, self.y_range_end, self.y_range_start)
    state[#state + 1] = self.send_sym
    state[#state + 1] = self.receive_sym

    if self.send_sym == "string" then
        state[#state + 1] = self.send_sym
    else
        state[#state + 1] = "empty"
    end

    if self.receive_sym == "string" then
        state[#state + 1] = self.receive_sym
    else
        state[#state + 1] = "empty"
    end

    state[#state + 1] = self.clip

    -- Call the outlet with the constructed list
    self:outlet(1, "list", state)
end

function circle:in_1_bang()
    local x = self:scale_value(self.slider_position_x, self.x_range_start, self.x_range_end)
    local y = self:scale_value(self.slider_position_y, self.y_range_end, self.y_range_start)
    self:outlet(1, "list", {x, y})
end

function circle:in_1_list(atoms)
    if atoms[1] == "_state" then
        self:restore_state(atoms)
        return
    end

    local width, height = self:get_size()
    self.slider_position_x = self:unscale_value(atoms[1], self.x_range_start, self.x_range_end)
    self.slider_position_y = self:unscale_value(atoms[2], self.y_range_end, self.y_range_start)
    self:save_state()
    self:repaint()
    self:in_1_bang()
end

function circle:in_1_set(atoms)
    self.slider_position_x = self:unscale_value(atoms[1], self.x_range_start, self.x_range_end)
    self.slider_position_y = self:unscale_value(atoms[2], self.y_range_end, self.y_range_start)
    self:repaint();
    self:save_state()
end

function circle:in_1_size(atoms)
    self:set_size(atoms[1], atoms[1])
    self:save_state()
end

function circle:in_1_range(atoms)
    self.x_range_start = atoms[1]
    self.x_range_end = atoms[2]
    self.y_range_end = atoms[1]
    self.y_range_start = atoms[2]
    self:save_state()
end

function circle:in_1_xrange(atoms)
    self.x_range_start = atoms[1]
    self.x_range_end = atoms[2]
    self:save_state()
end

function circle:in_1_yrange(atoms)
    self.y_range_end = atoms[1]
    self.y_range_start = atoms[2]
    self:save_state()
end

function circle:in_1_grid(atoms)
    self.grid = atoms[1]
    self:repaint()
    self:save_state()
end

function circle:in_1_shift(atoms)
    local width, height = self:get_size()
    self.mouse_down_x = self.last_mouse_x
    self.mouse_down_y = self.last_mouse_y
    self.mouse_down_slider_x = self.slider_position_x * width
    self.mouse_down_slider_y = self.slider_position_y * height
    self.shift = atoms[1]
end

function circle:in_1_bgcolor(atoms)
    self.bg1 = atoms
    self.bg2 = atoms
    self:repaint()
    self:save_state()
end

function circle:in_1_bgcolor1(atoms)
    self.bg1 = atoms
    self:repaint()
    self:save_state()
end

function circle:in_1_bgcolor2(atoms)
    self.bg2 = atoms
    self:repaint()
    self:save_state()
end

function circle:in_1_fgcolor(atoms)
    self.fg = atoms
    self:repaint()
    self:save_state()
end

function circle:in_1_savestate(atoms)
    self.savestate = atoms[1]
    self:save_state()
end

function circle:in_1_send(atoms)
    self.send_sym = atoms[1]
    self:save_state()
end

function circle:in_1_receive(atoms)
    self.receive_sym = atoms[1]
    self:save_state()
end

function circle:in_1_clip(atoms)
    self.clip = atoms[1]
    self:save_state()
end

function circle:in_1_jump(atoms)
    self.jump = atoms[1]
    self:save_state()
end

function circle:in_1_mode(atoms)
    self.mode = atoms[1]
    self:repaint()
    self:save_state()
end

function circle:scale_value(value, min, max)
    return min + (max - min) * value
end

function circle:unscale_value(value, min, max)
    return (value - min) / (max - min)
end

function circle:paint(g)
    local width, height = self:get_size()
    local x = self.slider_position_x * width
    local y = (1.0 - self.slider_position_y) * height

   if type(self.bg1) == "number" then
        g:set_color(self.bg1)
    elseif type(self.bg1) == "table" then
        g:set_color(table.unpack(self.bg1))
    end

    g:fill_all()

    if type(self.bg2) == "number" then
        g:set_color(self.bg2)
    elseif type(self.bg2) == "table" then
        g:set_color(table.unpack(self.bg2))
    end

    g:fill_ellipse(1, 1, width - 2, height - 2)

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

    if self.mode == 1 then
        g:draw_line(x, y, x, height / 2, 2)
        g:draw_line(x, y, width / 2, y, 2)
    elseif self.mode == 2 then
        g:draw_line(x, y, width / 2, height / 2, 2)
    end
end
