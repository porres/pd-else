local dial = pd.Class:new():register("dial")

-- creation arguments: phase size bg-color fg-color
-- bg-color and fg-color must be rgb triplets or 0|1 -1 -1
-- missing arguments will be set to defaults (0 127 0 -1 -1 1 -1 -1)
function dial:initialize(sel, atoms)
   self.inlets = 1
   self.outlets = 1
   -- normalized phase angle (divided by pi, 0 = center up, +/- 1 = center down)
   self.phase = 0
   -- foreground and background colors
   self.bg = Colors.background
   self.fg = Colors.foreground
   -- object size
   self:set_size(127, 127)
   -- restore state from creation arguments
   self:restore_state(atoms)
   return true
end

-- output the current phase value
function dial:in_1_bang()
   self:outlet(1, "float", {self.phase})
end

-- set the phase value
function dial:in_1_float(x)
   self.phase = x
   self:repaint()
   self:save_state()
end

-- set the object size
function dial:in_1_size(atoms)
   if type(atoms[1]) == "number" then
      self:set_size(atoms[1], atoms[1])
      self:save_state()
   end
end

-- set the fg and bg colors of the dial; the arguments must be either a single
-- number to indicate an indexed color from the palette, or an rgb triplet
function dial:in_1_fg(atoms)
   local fg = #atoms==1 and atoms[1] or #atoms==3 and atoms;
   if fg then
      self.fg = fg
      self:repaint()
      self:save_state()
   end
end

function dial:in_1_bg(atoms)
   local bg = #atoms==1 and atoms[1] or #atoms==3 and atoms;
   if bg then
      self.bg = bg
      self:repaint()
      self:save_state()
   end
end

-- calculate the x, y position of the tip of the hand
function dial:tip()
   local width, height = self:get_size()
   local x, y = math.sin(self.phase*math.pi), -math.cos(self.phase*math.pi)
   x, y = (x/2*0.8+0.5)*width, (y/2*0.8+0.5)*height
   return x, y
end

-- drag the hand with the mouse
function dial:mouse_down(x, y)
   self.tip_x, self.tip_y = self:tip()
end

function dial:mouse_drag(x, y)
   local width, height = self:get_size()

   local x1, y1 = x/width-0.5, y/height-0.5
   -- calculate the normalized phase, shifted by 0.5, since we want zero to be
   -- the center up position
   local phase = math.atan(y1, x1)/math.pi + 0.5
   -- renormalize if we get an angle > 1, to account for the phase shift
   if phase > 1 then
      phase = phase - 2
   end

   self.phase = phase

   local tip_x, tip_y = self:tip();

   if tip_x ~= self.tip_x or tip_y ~= self.tip_y then
      self.tip_x = tip_x
      self.tip_y = tip_y
      self:in_1_bang()
      self:repaint()
   end
end

function dial:mouse_up(x, y)
   self:save_state()
end

-- save and restore the state of this object instance
function dial:save_state()
   local width, height = self:get_size()
   local state = { self.phase, width }

   local function append_color(color)
      if type(color) == "number" then
	 table.move({ color, -1, -1 }, 1, 3, #state+1, state)
      elseif type(color) == "table" then
	 table.move(color, 1, 3, #state+1, state)
      end
   end

   append_color(self.bg)
   append_color(self.fg)

   -- save the state in our creation arguments
   self:set_args(state)
end

function dial:restore_state(atoms)
   if #atoms > 0 then
      local defaults = { 0, 127, 0, -1, -1, 1, -1, -1 }
      -- fill in missing argumens with defaults
      table.move(defaults, #atoms+1, #defaults, #atoms+1, atoms)
   end
   if #atoms >= 8 then
      for i = 1, 8 do
	 if type(atoms[i]) ~= "number" then
	    self:error(string.format("dial: argument #%d: expected number, got %s", i, type(atoms[i])))
	    return
	 end
      end
      -- restore the state from our creation arguments
      self.phase = atoms[1]
      local size = atoms[2]

      self.bg = atoms[4]==-1 and atoms[3] or {atoms[3], atoms[4], atoms[5]}
      self.fg = atoms[7]==-1 and atoms[6] or {atoms[6], atoms[7], atoms[8]}

      self:set_size(size, size)
      self:repaint()
   end
end

-- draw the dial
function dial:paint(g)
   local width, height = self:get_size()
   local x, y = self:tip()

   -- standard object border, fill with bg color
   g:set_color(0)
   g:fill_all()

   -- helper function to set a color value
   function set_color(x)
      if type(x) == "table" then
	 g:set_color(table.unpack(x))
      elseif type(x) == "number" then
	 g:set_color(x)
      end
   end

   -- draw circle and hand of the dial in the set colors
   set_color(self.bg)
   g:fill_ellipse(2, 2, width - 4, height - 4)
   set_color(self.fg)
   g:stroke_ellipse(2, 2, width - 4, height - 4, 4)
   g:fill_ellipse(width/2 - 3.5, height/2 - 3.5, 7, 7)
   g:draw_line(x, y, width/2, height/2, 2)
end
