local hello = pd.Class:new():register("hello-gui")

-- rendering speed (slows down rendering by the given factor)
-- In most cases it should be fine to just set this to 1 to run at full speed,
-- if you have a modern high-speed CPU and GPU. But we use a larger value as
-- default here to deal with low frame rates on some systems. You may have to
-- increase the value even further when running on low-spec systems like the
-- Raspberry Pi.
local R = 3

function hello:initialize(sel, atoms)
    self.inlets = 1

    self.circle_x = 485
    self.circle_y = 15
    self.circle_radius = 15
    self.animation_speed = 2*R
    self.delay_time = 16*R

    self.draggable_rect_x = 550
    self.draggable_rect_y = 130
    self.draggable_rect_size = 50
    self.dragging_rect = false
    self.mouse_down_pos = {0, 0}
    self.rect_down_pos = {0, 0}

    self:set_size(630, 230)
    return true
end

function math.clamp(val, lower, upper)
    if lower > upper then lower, upper = upper, lower end -- swap if boundaries supplied the wrong way
    return math.max(lower, math.min(upper, val))
end

function hello:postinitialize()
    self.clock = pd.Clock:new():register(self, "tick")
    self.clock:delay(self.delay_time)
end

function hello:finalize()
    self.clock:destruct()
end

function hello:mouse_down(x, y)
    if x > self.draggable_rect_x and y > self.draggable_rect_y and x < self.draggable_rect_x + self.draggable_rect_size and y < self.draggable_rect_y + self.draggable_rect_size then
        dragging_rect = true
        self.mouse_down_pos[0] = x
        self.mouse_down_pos[1] = y
        self.rect_down_pos[0] = self.draggable_rect_x
        self.rect_down_pos[1] = self.draggable_rect_y
    else
        dragging_rect = false
    end
end

function hello:mouse_drag(x, y)
    if dragging_rect == true then
        self.draggable_rect_x = self.rect_down_pos[0] + (x - self.mouse_down_pos[0])
        self.draggable_rect_y = self.rect_down_pos[1] + (y - self.mouse_down_pos[1])
        self.draggable_rect_x = math.clamp(self.draggable_rect_x, 0, 620 - self.draggable_rect_size)
        self.draggable_rect_y = math.clamp(self.draggable_rect_y, 0, 230 - self.draggable_rect_size)
        self:repaint(3)
    end
end

function hello:paint(g)
    g:set_color(50, 50, 50)
    g:fill_all()

    -- Filled examples
    g:set_color(66, 207, 201, 0.3)
    g:fill_ellipse(30, 50, 30, 30)
    g:set_color(0, 159, 147, 1)
    g:fill_rect(120, 50, 30, 30)
    g:set_color(250, 84, 108, 1)
    g:fill_rounded_rect(210, 50, 30, 30, 5)

    g:set_color(252, 118, 81, 1)

    -- Star using line_to paths
    local starX1, starY1 = 310, 45
    local starSize = 15

    local star = Path(starX1, starY1)

    -- Star using line_to paths
    star:line_to(starX1 + 5, starY1 + 14)
    star:line_to(starX1 + 20, starY1 + 14)
    star:line_to(starX1 + 8, starY1 + 22)
    star:line_to(starX1 + 14, starY1 + 36)
    star:line_to(starX1, starY1 + 27)
    star:line_to(starX1 - 14, starY1 + 36)
    star:line_to(starX1 - 6, starY1 + 22)
    star:line_to(starX1 - 18, starY1 + 14)
    star:line_to(starX1 - 3, starY1 + 14)
    star:close()

    g:fill_path(star)

    g:set_color(255, 219, 96, 1)
    -- Bezier curve example
    g:translate(140, 20)
    g:scale(0.5, 1.0)
    local curve = Path(450, 50)
    curve:cubic_to(500, 30, 550, 70, 600, 50)
    curve:close()
    g:stroke_path(curve, 2)
    g:reset_transform()

    -- Stroked examples
    g:set_color(66, 207, 201, 1)
    g:stroke_ellipse(30, 150, 30, 30, 2)
    g:set_color(0, 159, 147, 1)
    g:stroke_rect(120, 150, 30, 30, 2)
    g:set_color(250, 84, 108, 1)
    g:stroke_rounded_rect(210, 150, 30, 30, 5, 2)

    g:set_color(252, 118, 81, 1)

    local starX2, starY2 = 310, 145
    local starSize = 15

    -- Star using line_to paths
    local star2 = Path(starX2, starY2)
    star2:line_to(starX2 + 5, starY2 + 14)
    star2:line_to(starX2 + 20, starY2 + 14)
    star2:line_to(starX2 + 8, starY2 + 22)
    star2:line_to(starX2 + 14, starY2 + 36)
    star2:line_to(starX2, starY2 + 27)
    star2:line_to(starX2 - 14, starY2 + 36)
    star2:line_to(starX2 - 6, starY2 + 22)
    star2:line_to(starX2 - 18, starY2 + 14)
    star2:line_to(starX2 - 3, starY2 + 14)
    star2:close()
    g:stroke_path(star2, 2)

    g:set_color(255, 219, 96, 1)
    -- Bezier curve example
    g:translate(140, 20)
    g:scale(0.5, 1.0)
    local curve2 = Path(450, 150)
    curve2:cubic_to(500, 130, 550, 170, 600, 150)
    g:fill_path(curve2)
    g:reset_transform()

    -- Titles
    g:set_color(252, 118, 81, 1)
    g:draw_text("Ellipse", 25, 190, 120, 12)
    g:draw_text("Rectangle", 100, 190, 120, 12)
    g:draw_text("Rounded Rect", 188, 190, 120, 12)
    g:draw_text("Paths", 295, 190, 120, 12)
    g:draw_text("Bezier Paths", 360, 190, 120, 12)
    g:draw_text("Animation", 460, 190, 120, 12)
    g:draw_text("   Mouse\nInteraction", 540, 190, 120, 12)
end

function hello:paint_layer_2(g)
    g:set_color(250, 84, 108, 1)
    g:fill_ellipse(self.circle_x, self.circle_y, self.circle_radius, self.circle_radius)
end

function hello:paint_layer_3(g)
    -- Draggable rectangle
    g:set_color(66, 207, 201, 1)
    g:fill_rounded_rect(self.draggable_rect_x, self.draggable_rect_y, self.draggable_rect_size, self.draggable_rect_size, 5)
    g:set_color(0, 0, 0, 1)
    g:draw_text("Drag\n me!", self.draggable_rect_x + 8, self.draggable_rect_y + 10, self.draggable_rect_size, 12)
end

function hello:tick()
    self.circle_y = self.circle_y + self.animation_speed
    if self.circle_y > 160 + self.circle_radius then
        self.circle_y = -self.circle_radius
    end
    self:repaint(2)
    self.clock:delay(self.delay_time)
end


function hello:in_1_bang()
    self:repaint()
end
