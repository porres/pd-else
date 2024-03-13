local scope3d = pd.Class:new():register("scope3d~")

function scope3d:initialize(sel, atoms)
  self.inlets = {SIGNAL, SIGNAL, SIGNAL}
  self.methods =
  {
    -- methods for use from messages or creation flags, state start index and #values
    --
    -- methods that allow variable arg count (like rotate) need to be 
    -- listed after the other methods for correct state restoring 
    rotatex     = {function(s, a) return s:pd_rotatex(a)     end, 1, 1},
    rotatey     = {function(s, a) return s:pd_rotatey(a)     end, 2, 1},
    rotate      = {function(s, a) return s:pd_rotate(a)      end, 1, 2},

    dim         = {function(s, a) return s:pd_dim(a)         end, 3, 2},

    zoom        = {function(s, a) return s:pd_zoom(a)        end, 5, 1},
    drag        = {function(s, a) return s:pd_drag(a)        end, 6, 1},
    grid        = {function(s, a) return s:pd_grid(a)        end, 7, 1},
    perspective = {function(s, a) return s:pd_perspective(a) end, 8, 1},
    width       = {function(s, a) return s:pd_width(a)       end, 9, 1},

    nlines      = {function(s, a) return s:pd_nlines(a)      end, 10, 1},
    nsamples    = {function(s, a) return s:pd_nsamples(a)    end, 11, 1},
    rate        = {function(s, a) return s:pd_rate(a)        end, 12, 1},

    fgcolor     = {function(s, a) return s:pd_fgcolor(a)     end, 13, 3},
    bgcolor     = {function(s, a) return s:pd_bgcolor(a)     end, 16, 3},
    gridcolor   = {function(s, a) return s:pd_gridcolor(a)   end, 19, 3},

    receive     = {function(s, a) return s:pd_receive(a)     end, 22, 1},

    range       = {function(s, a) return s:pd_range(a)       end, 23, 4},
    xrange      = {function(s, a) return s:pd_xrange(a)      end, 23, 2},
    yrange      = {function(s, a) return s:pd_yrange(a)      end, 25, 2},

    list        = {function(s, a) return s:pd_list(a)        end},
  }
  self:reset_state()
  self:reset_data()

  local kwargs, args = self:handle_args(atoms)
  for k, v in pairs(kwargs) do
    self:call_pd_method(k, v)
  end
  self:restore_state(args)
  return true
end

function scope3d:call_pd_method(sel, atoms)
  local func = self.methods[sel] and self.methods[sel][1]
  if(func) then
    if self.methods[sel][2] then
      for i, atom in ipairs(atoms) do
        -- write atoms to state index
        self.state[self.methods[sel][2] + i-1] = atom
      end
    end
    self:save_state()
    func(self, atoms)
  end
end

function scope3d:restore_state(atoms)
  for key, method in pairs(self.methods) do
    if method[2] then -- if method has state index
      local args = {}
      for i = method[2], method[2] + method[3]-1 do
        if atoms[i] ~= "empty" then table.insert(args, atoms[i]) end
      end
      if #args > 0 then self:call_pd_method(key, args) end
    end 
  end
end

function scope3d:save_state()
  self:set_args(self.state)
end

function scope3d:reset_state()
  self.state = {}
  local count = 0
  -- get state args count
  for _, method in pairs(self.methods) do
    if method[2] then
      count = math.max(count, method[2] + method[3] - 1)
    end
  end
  for i = 1, count do
    table.insert(self.state, "empty")
  end
end

function scope3d:reset_data()
  self.gridLines = self:create_grid(-1, 1, 0.25)
  self.cameraDistance = 6
  self.bufferIndex = 1
  self.sampleIndex = 1
  self.rotationAngleX, self.rotationAngleY = 0, 0
  self.rotationStartAngleX, self.rotationStartAngleY = 0, 0

  self.FGCOLOR = {30, 30, 30} -- was Color.foreground for plugdata
  self.BGCOLOR = {190, 190, 190} -- was Colors.background for plugdata

  self.WIDTH, self.HEIGHT = 140, 140
  self.RATE = 20
  self.BUFFERSIZE = 256
  self.SAMPLING_INTERVAL = 8
  self.DRAW_GRID = 1
  self.DRAG = 1
  self.STROKE_WIDTH = 1
  self.ZOOM = 1
  self.GRIDCOLOR = {160, 160, 160}
  self.PERSPECTIVE = 1
  self.XRANGEFROM, self.XRANGETO = -1, 1
  self.YRANGEFROM, self.YRANGETO = -1, 1
  self:set_size(self.WIDTH, self.HEIGHT)
  self:reset_buffer()
end

function scope3d:reset_buffer()
  self.signal = {}
  self.rotatedSignal = {}
  -- prefill ring buffer
  for i = 1, self.BUFFERSIZE do 
    self.signal[i] = {0, 0, 0}
    self.rotatedSignal[i] = {0, 0, 0}
  end
end

function scope3d:postinitialize()
  self.clock = pd.Clock:new():register(self, "tick")
  self.clock:delay(self.RATE)
end

function scope3d:finalize()
  self.clock:destruct()
  if self.recv then self.recv:destruct() end
end

function scope3d:handle_args(atoms)
  local kwargs = {}
  local args = {}
  local collectKey = nil
  for _, atom in ipairs(atoms) do
    if type(atom) ~= "number" and string.sub(atom, 1, 1) == "-" then
      -- start collecting values for a new key if key detected
      collectKey = string.sub(atom, 2)
      kwargs[collectKey] = {}
    elseif collectKey then
      -- if currently collecting values for a key, add this atom to that key's table
      table.insert(kwargs[collectKey], atom)
    else
      -- otherwise treat as a positional argument
      table.insert(args, atom)
    end
  end
  return kwargs, args
end

function scope3d:tick()
  self:repaint()
  self.clock:delay(self.RATE)
end

function scope3d:create_grid(minVal, maxVal, step)
  local grid = {}
  for i = minVal, maxVal, step do
    table.insert(grid, {{i, 0, minVal}, {i, 0, maxVal}})
    table.insert(grid, {{minVal, 0, i}, {maxVal, 0, i}})
  end
  return grid
end

function scope3d:mouse_down(x, y)
  if self.DRAG == 1 then self.dragStartX, self.dragStartY = x, y end
end

function scope3d:mouse_up(x, y)
  if self.DRAG == 1 then 
    self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
  end
end

function scope3d:mouse_drag(x, y)
  if self.DRAG == 1 then 
    self.rotationAngleY = self.rotationStartAngleY + ((x-self.dragStartX) / 2)
    self.rotationAngleX = self.rotationStartAngleX - ((y-self.dragStartY) / 2)
  end
end

function scope3d:dsp(samplerate, blocksize)
    self.blocksize = blocksize
end

function scope3d:perform(in1, in2, in3)
  while self.sampleIndex <= self.blocksize do
    -- circular buffer
    self.signal[self.bufferIndex] = {in1[self.sampleIndex], in2[self.sampleIndex], in3[self.sampleIndex]}
    self.bufferIndex = (self.bufferIndex % self.BUFFERSIZE) + 1
    self.sampleIndex = self.sampleIndex + self.SAMPLING_INTERVAL
  end
  self.sampleIndex = self.sampleIndex - self.blocksize
end

function scope3d:paint(g)
  g:set_color(table.unpack(self.BGCOLOR))
  g:fill_all()

  -- draw ground grid
  if self.DRAW_GRID == 1 then
    g:set_color(table.unpack(self.GRIDCOLOR))
    for i = 1, #self.gridLines do
      local lineFrom, lineTo = table.unpack(self.gridLines[i])
      
      -- apply rotation to grid lines
      lineFrom = self:rotate_y(lineFrom, self.rotationAngleY)
      lineFrom = self:rotate_x(lineFrom, self.rotationAngleX)
      lineTo   = self:rotate_y(lineTo  , self.rotationAngleY)
      lineTo   = self:rotate_x(lineTo  , self.rotationAngleX)

      local startX, startY = self:projectVertex(lineFrom, self.ZOOM)
      local   endX,   endY = self:projectVertex(  lineTo, self.ZOOM)

      if lineFrom[3] > -self.cameraDistance and lineTo[3] > -self.cameraDistance then
        g:draw_line(startX, startY, endX, endY, 1)
      end
    end
  end

  for i = 1, self.BUFFERSIZE do
    local offsetIndex = (i + self.bufferIndex-2) % self.BUFFERSIZE + 1
    local rotatedVertex = self:rotate_y(self.signal[offsetIndex], self.rotationAngleY)
    self.rotatedSignal[i] = self:rotate_x(rotatedVertex, self.rotationAngleX)
  end

  g:set_color(table.unpack(self.FGCOLOR))
  local p = Path(self:projectVertex(self.rotatedSignal[1], self.ZOOM))
  for i = 2, self.BUFFERSIZE do
    p:line_to(self:projectVertex(self.rotatedSignal[i], self.ZOOM))
  end
  g:stroke_path(p, self.STROKE_WIDTH)
end

function scope3d:rotate_y(vertex, angle)
  local x, y, z = table.unpack(vertex)
  local cosTheta = math.cos(angle * math.pi / 180)
  local sinTheta = math.sin(angle * math.pi / 180)
  local newX = x * cosTheta - z * sinTheta
  local newZ = x * sinTheta + z * cosTheta
  return {newX, y, newZ}
end

function scope3d:rotate_x(vertex, angle)
  local x, y, z = table.unpack(vertex)
  local cosTheta = math.cos(angle * math.pi / 180)
  local sinTheta = math.sin(angle * math.pi / 180)
  local newY = y * cosTheta - z * sinTheta
  local newZ = y * sinTheta + z * cosTheta
  return {x, newY, newZ}
end

function scope3d:projectVertex(vertex)
  -- local minDim = math.min(self.WIDTH - 2, self.HEIGHT - 2)
  local scale = self.cameraDistance / (self.cameraDistance + vertex[3] * self.PERSPECTIVE)
  local screenX = self.WIDTH / 2 + 0.5 + (vertex[1] * scale * self.ZOOM * self.WIDTH * 0.5)
  local screenY = self.HEIGHT / 2 + 0.5 - (vertex[2] * scale * self.ZOOM * self.HEIGHT * 0.5)
  screenX = self:mapRange(screenX, self.XRANGEFROM, self.XRANGETO)
  screenY = self:mapRange(screenY, self.YRANGEFROM, self.YRANGETO) 
  return screenX, screenY
end

function scope3d:mapRange(x, xrangeFrom, xrangeTo)
  local xNew = (x + 1) * (xrangeTo - xrangeFrom) * 0.5 + xrangeFrom
  pd.post('range '..xrangeFrom..' '..xrangeTo..' '..x..' '..xNew)
  return xNew
end

function scope3d:in_1(sel, atoms)
  self:call_pd_method(sel, atoms)
end

function scope3d:receive(sel, atoms)
  self:call_pd_method(sel, atoms)
end

-- /////////////////////////////////////////////////////////////

function scope3d:pd_receive(x)
  if self.recv then self.recv:destruct() end
  if x[1] then
    self.recv = pd.Receive:new():register(self, tostring(x[1]), "receive")
  end
end

function scope3d:pd_rotatex(x)
  if type(x[1]) == "number" then
    self.rotationAngleY = x[1]
    self.rotationStartAngleY = self.rotationAngleY
  end
end

function scope3d:pd_rotatey(x)
  if type(x[1]) == "number" then
    self.rotationAngleX = x[1]
    self.rotationStartAngleX = self.rotationAngleX
  end
end

function scope3d:pd_rotate(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.rotationAngleX, self.rotationAngleY = x[2], x[1]
    self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
  end
end

function scope3d:pd_range(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.XRANGEFROM = x[1]
    self.XRANGETO   = x[2]
    self.YRANGEFROM = x[1]
    self.YRANGETO   = x[2]
  end
end

function scope3d:pd_xrange(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.XRANGEFROM = x[1]
    self.XRANGETO   = x[2]
  end
end

function scope3d:pd_yrange(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.YRANGEFROM = x[1]
    self.YRANGETO   = x[2]
  end
end

function scope3d:pd_zoom(x)
  self.ZOOM = type(x[1]) == "number" and x[1] or 1
end

function scope3d:pd_drag(x)
  self.DRAG = type(x[1]) == "number" and x[1] or 1 - self.DRAG
end

function scope3d:pd_grid(x)
  self.DRAW_GRID = type(x[1]) == "number" and x[1] or 1 - self.DRAW_GRID
end

function scope3d:pd_dim(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    local width = math.max(20, x[1])
    local height = math.max(20, x[2])
    self.WIDTH = width
    self.HEIGHT = height
    self:set_size(self.WIDTH, self.HEIGHT)
  end
end

function scope3d:pd_nlines(x)
  if type(x[1]) == "number" then
    self.BUFFERSIZE = math.min(1024, math.max(2, math.floor(x[1])))
    self:reset_buffer()
  end
end

function scope3d:pd_nsamples(x)
  if type(x[1]) == "number" then
    self.SAMPLING_INTERVAL = math.max(2, math.floor(x[1])) - 1
  end
end

function scope3d:pd_list(x)
  self:call_pd_method("nsamples", {x[1]})
  if #x >= 2 then
    self:call_pd_method("nlines", {x[2]})
  end
end

function scope3d:pd_width(x)
  self.STROKE_WIDTH = type(x[1]) == "number" and math.max(1, x[1]) or 1
end

function scope3d:pd_perspective(x)
  self.PERSPECTIVE = type(x[1]) == "number" and x[1] or 1
end

function scope3d:pd_rate(x)
  if type(x[1]) == "number" then
    self.RATE = math.max(8, x[1])
    if self.clock then
      self.clock:unset()
      self.clock:delay(self.RATE)
    end
  end
end

function scope3d:pd_fgcolor(x)
  if #x == 1 then x = {x[1], x[1], x[1]} end
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.FGCOLOR = {x[1], x[2], x[3]}
  end
end

function scope3d:pd_bgcolor(x)
  if #x == 1 then x = {x[1], x[1], x[1]} end
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.BGCOLOR = {x[1], x[2], x[3]}
  end
end

function scope3d:pd_gridcolor(x)
  if #x == 1 then x = {x[1], x[1], x[1]} end
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.GRIDCOLOR = {x[1], x[2], x[3]}
  end
end