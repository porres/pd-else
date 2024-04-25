local pdlua_flames = {}
local scope3d = pd.Class:new():register("scope3d~")

function scope3d:initialize(name, args)
  self.inlets = {SIGNAL, SIGNAL, SIGNAL}
  local methods =
  {
    { name = "rotate",      defaults = { 0, 0 } },
    { name = "rotatex" },
    { name = "rotatey" },

    { name = "dim",         defaults = { 140.0, 140.0 } },

    { name = "width",       defaults = { 1 } },
    { name = "clip",        defaults = { 0 } },
    { name = "grid",        defaults = { 1 } },
    { name = "drag",        defaults = { 1 } },
    { name = "zoom",        defaults = { 1 } },
    { name = "perspective", defaults = { 1 } },
    { name = "rate",        defaults = { 20 } },

    { name = "list",        defaults = { 8, 256 } },
    { name = "nsamples" },
    { name = "nlines" },

    { name = "fgcolor",     defaults = { 30, 30, 30 } },
    { name = "bgcolor",     defaults = { 190, 190, 190 } },
    { name = "gridcolor",   defaults = { 160, 160, 160 } },
    { name = "receive",     defaults = { nil } },
  }
  self.cameraDistance = 6
  self.rotationAngleX, self.rotationAngleY = 0, 0
  self.rotationStartAngleX, self.rotationStartAngleY = 0, 0
  self.gridLines = self:create_grid(-1, 1, 0.25)

  self.pd_env = { ignorewarnings = true }
  pdlua_flames:init_pd_methods(self, name, methods, args)
  self:reset_buffer()
  return true
end

function scope3d:create_grid(minVal, maxVal, step)
  local grid = {}
  for i = minVal, maxVal, step do
    table.insert(grid, {i, 0, minVal, i, 0, maxVal})
    table.insert(grid, {minVal, 0, i, maxVal, 0, i})
  end
  return grid
end

function scope3d:reset_buffer()
  self.bufferIndex = 1
  self.displayBufferIndex = 1
  self.sampleIndex = 1
  self.displaySignalX, self.displaySignalY, self.displaySignalZ = {}, {}, {}
  self.signalX, self.signalY, self.signalZ = {}, {}, {}
  self.rotatedX, self.rotatedY, self.rotatedZ = {}, {}, {}
  -- prefill ring buffer
  for i = 1, self.nlines do 
    self.signalX[i], self.signalY[i], self.signalZ[i] = 0, 0, 0
    self.rotatedX[i], self.rotatedY[i], self.rotatedZ[i] = 0, 0, 0
  end
  self:update_lines()
end

function scope3d:postinitialize()
  self.clock = pd.Clock:new():register(self, "tick")
  self.clock:delay(self.frameDelay)
end

function scope3d:tick()
  self.width, self.height = self:get_size()
  self:update_lines()
  self:repaint()
  self.clock:delay(self.frameDelay)
end

function scope3d:finalize()
  self.clock:destruct()
  if self.recv then self.recv:destruct() end
end

function scope3d:pd_width(x)
  if type(x[1]) == "number" then
    self.strokeWidth = x[1]
  end
end

function scope3d:pd_zoom(x)
  if type(x[1]) == "number" then 
    self.zoom = x[1]
  end
end

function scope3d:pd_perspective(x)
  if type(x[1]) == "number" then 
    self.perspective = x[1]
  end
end

function scope3d:pd_grid(x)
  if type(x[1]) == "number" then
    self.grid = x[1]
  end
end

function scope3d:pd_clip(x)
  if type(x[1]) == "number" then self.clip = x[1] end
end

function scope3d:pd_drag(x)
  if type(x[1]) == "number" then self.drag = x[1] end
end

function scope3d:pd_gridcolor(x)
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.gridColorR, self.gridColorG, self.gridColorB = table.unpack(x)
  end
end

function scope3d:pd_bgcolor(x)
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.bgColorR, self.bgColorG, self.bgColorB = table.unpack(x)
  end
end

function scope3d:pd_fgcolor(x)
  if #x == 3 and
     type(x[1]) == "number" and
     type(x[2]) == "number" and
     type(x[3]) == "number" then
    self.fgColorR, self.fgColorG, self.fgColorB = table.unpack(x)
  end
end

function scope3d:pd_rate(x)
  if type(x[1]) == "number" then
    self.frameDelay = math.max(8, x[1])
  end
  if self.clock then
    self.clock:unset() 
    self.clock:delay(self.frameDelay)
  end
end

function scope3d:pd_list(x)
  self.nsamples = (math.max(2, math.floor(x[1])) - 1) or 8
  self.nlines = math.min(1024, math.max(2, math.floor(x[2]))) or 256
  self:reset_buffer()
end

function scope3d:pd_nsamples(x)
  self.pd_methods.list.val[1] = x[1]
  self:handle_pd_message('list', self.pd_methods.list.val)
end

function scope3d:pd_nlines(x)
  self.pd_methods.list.val[2] = x[1]
  self:handle_pd_message('list', self.pd_methods.list.val)
end

function scope3d:pd_rotate(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.rotationAngleY, self.rotationAngleX = x[1], x[2]
    self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
  end
end

function scope3d:pd_rotatex(x)
  if type(x[1]) == "number" then
    local rotation = {self.rotationAngleY, self.rotationAngleX}
    rotation[1] = x[1]
    self:handle_pd_message('rotate', rotation)
  end
end

function scope3d:pd_rotatey(x)
  if type(x[1]) == "number" then
    local rotation = {self.rotationAngleY, self.rotationAngleX}
    rotation[2] = x[1]
    self:handle_pd_message('rotate', rotation)
  end
end

function scope3d:pd_dim(x)
  if #x == 2 and
     type(x[1]) == "number" and
     type(x[2]) == "number" then
    self.width, self.height = math.floor(x[1]), math.floor(x[2])
    self.widthMinusOne, self.heightMinusOne = self.width-1, self.height-1
    self:set_size(self.width, self.height)
  end
end

function scope3d:dsp(sr, bs)
    self.blocksize = bs
end

function scope3d:mouse_down(x, y)
  if self.drag == 1 then self.dragStartX, self.dragStartY = x, y end
end

function scope3d:mouse_up(x, y)
  if self.drag == 1  then
    self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
  end
end

function scope3d:mouse_drag(x, y)
  if self.drag == 1 then 
    self.rotationAngleY = self.rotationStartAngleY + ((x-self.dragStartX) / 2)
    self.rotationAngleX = self.rotationStartAngleX + ((-y+self.dragStartY) / 2)
    self:repaint()
  end
end

function scope3d:perform(in1, in2, in3)
  while self.sampleIndex <= self.blocksize do
    -- ring buffer
    self.signalX[self.bufferIndex] = in1[self.sampleIndex]
    self.signalY[self.bufferIndex] = in2[self.sampleIndex]
    self.signalZ[self.bufferIndex] = in3[self.sampleIndex]
    self.bufferIndex = (self.bufferIndex % self.nlines) + 1
    self.sampleIndex = self.sampleIndex + self.nsamples
  end
  self.sampleIndex = self.sampleIndex - self.blocksize
end

function scope3d:clipLine(x1, y1, x2, y2)
  local function isInside(x, y)
    return x >= 1 and x <= self.widthMinusOne and y >= 1 and y <= self.heightMinusOne
  end

  local function computeOutCode(x, y)
    local code = 0
    if x < 1 then -- to the left of clip window
      code = code | 1
    elseif x > self.widthMinusOne then -- to the right of clip window
      code = code | 2
    end
    if y < 1 then -- below the clip window
      code = code | 4
    elseif y > self.heightMinusOne then -- above the clip window
      code = code | 8
    end
    return code
  end

  local function clipPoint(x, y, outcode)
    local dx = x2 - x1
    local dy = y2 - y1

    if outcode & 8 ~= 0 then -- point is above the clip window
      x = x1 + dx * (self.heightMinusOne - y) / dy
      y = self.heightMinusOne
    elseif outcode & 4 ~= 0 then -- point is below the clip window
      x = x1 + dx * (1 - y) / dy
      y = 1
    elseif outcode & 2 ~= 0 then -- point is to the right of clip window
      y = y1 + dy * (self.widthMinusOne - x) / dx
      x = self.widthMinusOne
    elseif outcode & 1 ~= 0 then -- point is to the left of clip window
      y = y1 + dy * (1 - x) / dx
      x = 1
    end
    return x, y
  end

  local outcode1 = computeOutCode(x1, y1)
  local outcode2 = computeOutCode(x2, y2)
  local accept = false

  while true do
    if outcode1 == 0 and outcode2 == 0 then -- both points inside
      accept = true
      break
    elseif (outcode1 & outcode2) ~= 0 then -- both points share an outside zone
      break
    else
      local x, y
      local outcodeOut = (outcode1 ~= 0) and outcode1 or outcode2

      x, y = clipPoint(x1, y1, outcodeOut)

      if outcodeOut == outcode1 then
        x1, y1 = x, y
        outcode1 = computeOutCode(x1, y1)
      else
        x2, y2 = x, y
        outcode2 = computeOutCode(x2, y2)
      end
    end
  end

  if accept then
    return x1, y1, x2, y2
  end
end

function scope3d:paint(g)
  g:set_color(self.bgColorR, self.bgColorG, self.bgColorB)
  g:fill_all()

  -- draw ground grid
  if self.grid == 1 then
    g:set_color(self.gridColorR, self.gridColorG, self.gridColorB)
    for i = 1, #self.gridLines do
      local lineFromX, lineFromY, lineFromZ, lineToX, lineToY, lineToZ = table.unpack(self.gridLines[i])
      
      -- apply rotation to grid lines
      lineFromX, lineFromY, lineFromZ = self:rotate_y(lineFromX, lineFromY, lineFromZ, self.rotationAngleY)
      lineFromX, lineFromY, lineFromZ = self:rotate_x(lineFromX, lineFromY, lineFromZ, self.rotationAngleX)
      lineToX, lineToY, lineToZ = self:rotate_y(lineToX, lineToY, lineToZ, self.rotationAngleY)
      lineToX, lineToY, lineToZ = self:rotate_x(lineToX, lineToY, lineToZ, self.rotationAngleX)

      local startX, startY = self:projectVertex(lineFromX, lineFromY, lineFromZ, self.zoom)
      local endX, endY = self:projectVertex(lineToX, lineToY, lineToZ, self.zoom)
      if self.clip == 1 then 
        startX, startY, endX, endY = self:clipLine(startX, startY, endX, endY)
        if startX then g:draw_line(startX, startY, endX, endY, self.strokeWidth) end
      else
        g:draw_line(startX, startY, endX, endY, 1)
      end
    end
  end

  for i = 1, self.nlines do
    local offsetIndex = (i + self.displayBufferIndex-2) % self.nlines + 1
    local rotatedX, rotatedY, rotatedZ = self:rotate_y(self.displaySignalX[offsetIndex], self.displaySignalY[offsetIndex], self.displaySignalZ[offsetIndex], self.rotationAngleY)
    self.rotatedX[i], self.rotatedY[i], self.rotatedZ[i] = self:rotate_x(rotatedX, rotatedY, rotatedZ, self.rotationAngleX)
  end

  g:set_color(self.fgColorR, self.fgColorG, self.fgColorB)
  if self.clip == 1 then 
    local startX, startY = self:projectVertex(self.rotatedX[1], self.rotatedY[1], self.rotatedZ[1], self.pd_methods.zoom.val[1])
    for i = 2, self.nlines do
      local endX, endY = self:projectVertex(self.rotatedX[i], self.rotatedY[i], self.rotatedZ[i], self.pd_methods.zoom.val[1])
      local prevX, prevY = endX, endY
      startX, startY, endX, endY = self:clipLine(startX, startY, endX, endY)
      if startX then g:draw_line(startX, startY, endX, endY, self.strokeWidth) end
      startX, startY = prevX, prevY
    end
  else
    local p = Path(self:projectVertex(self.rotatedX[1], self.rotatedY[1], self.rotatedZ[1], self.zoom))
    for i = 2, self.nlines do
      p:line_to(self:projectVertex(self.rotatedX[i], self.rotatedY[i], self.rotatedZ[i], self.zoom))
    end
    g:stroke_path(p, self.strokeWidth)
  end
end

function scope3d:update_lines()
  self.displayBufferIndex = self.bufferIndex
  for i = 1, self.nlines do
    self.displaySignalX[i], self.displaySignalY[i], self.displaySignalZ[i] = self.signalX[i], self.signalY[i], self.signalZ[i]
  end
end

function scope3d:rotate_y(x, y, z, angle)
  local cosTheta = math.cos(angle * math.pi / 180)
  local sinTheta = math.sin(angle * math.pi / 180)
  local newX = x * cosTheta - z * sinTheta
  local newZ = x * sinTheta + z * cosTheta
  return newX, y, newZ
end

function scope3d:rotate_x(x, y, z, angle)
  local cosTheta = math.cos(angle * math.pi / 180)
  local sinTheta = math.sin(angle * math.pi / 180)
  local newY = y * cosTheta - z * sinTheta
  local newZ = y * sinTheta + z * cosTheta
  return x, newY, newZ
end

function scope3d:projectVertex(x, y, z)
  local maxDim = math.max(self.width - 2, self.height - 2)
  local scale = self.cameraDistance / (self.cameraDistance + z * self.perspective)
  local screenX = self.width / 2 + 0.5 + (x * scale * self.zoom * maxDim * 0.5)
  local screenY = self.height / 2 + 0.5 - (y * scale * self.zoom * maxDim * 0.5)
  return screenX, screenY
end

function scope3d:receive(sel, atoms)
  self:handle_pd_message(sel, atoms)
end

function scope3d:in_n(n, sel, atoms)
  if n == 1 then
    self:handle_pd_message(sel, atoms, 1)
  else
    local baseMessage = self.pd_env.name .. ': no method for \'' .. sel .. '\''
    local inletMessage = n and ' on inlet ' .. string.format('%d', n) or ''
    self:error(baseMessage .. inletMessage)
  end
end

function scope3d:pd_receive(x)
  if x[1] and x[1] ~= "empty" then
    if self.recv then self.recv:destruct() end
    self.recv = pd.Receive:new():register(self, tostring(x[1]), "receive")
  end
end

function pdlua_flames:init_pd_methods(pdclass, name, methods, atoms)
  pdclass.handle_pd_message = pdlua_flames.handle_pd_message
  pdclass.pd_env = pdclass.pd_env or {}
  pdclass.pd_env.name = name
  pdclass.pd_methods = {}
  pdclass.pd_args = {}

  local kwargs, args = self:parse_atoms(atoms)
  local argIndex = 1
  for _, method in ipairs(methods) do
    pdclass.pd_methods[method.name] = {}
    -- add method if a corresponding method exists
    local pd_method_name = 'pd_' .. method.name
    if pdclass[pd_method_name] and type(pdclass[pd_method_name]) == 'function' then
      pdclass.pd_methods[method.name].func = pdclass[pd_method_name]
    elseif not pdclass.pd_env.ignorewarnings then
      pdclass:error(name..': no function \'' .. pd_method_name .. '\' defined')
    end
    -- process initial defaults
    if method.defaults then
      -- initialize entry for storing index and arg count and values
      pdclass.pd_methods[method.name].index = argIndex
      pdclass.pd_methods[method.name].arg_count = #method.defaults
      pdclass.pd_methods[method.name].val = method.defaults
      -- populate pd_args with defaults or preset values
      local argValues = {}
      for _, value in ipairs(method.defaults) do
        local addArg = args[argIndex] or value
        pdclass.pd_args[argIndex] = addArg
        table.insert(argValues, addArg)
        argIndex = argIndex + 1
      end
      pdclass:handle_pd_message(method.name, argValues)
    end
  end
  for sel, atoms in pairs(kwargs) do
    pdclass:handle_pd_message(sel, atoms)
  end
end

---------------------------------------------------------------------------------------------

function pdlua_flames:parse_atoms(atoms)
  local kwargs = {}
  local args = {}
  local collectKey = nil
  for _, atom in ipairs(atoms) do
    if type(atom) ~= 'number' and string.sub(atom, 1, 1) == '-' then
      collectKey = string.sub(atom, 2)
      kwargs[collectKey] = {}
    elseif collectKey then
      table.insert(kwargs[collectKey], atom)
    else
      table.insert(args, atom)
    end
  end
  return kwargs, args
end

function pdlua_flames:handle_pd_message(sel, atoms, n)
  if self.pd_methods[sel] then
    local startIndex = self.pd_methods[sel].index
    local valueCount = self.pd_methods[sel].arg_count
    local returnValues = self.pd_methods[sel].func and self.pd_methods[sel].func(self, atoms)
    local values = {}
    if startIndex and valueCount then
      for i, atom in ipairs(returnValues or atoms) do
        if i > valueCount then break end
        table.insert(values, atom)
        self.pd_args[startIndex + i-1] = atom
      end
    end
    self.pd_methods[sel].val = values
    self:set_args(self.pd_args)
  else
    local baseMessage = self.pd_env.name .. ': no method for \'' .. sel .. '\''
    local inletMessage = n and ' on inlet ' .. string.format('%d', n) or ''
    self:error(baseMessage .. inletMessage)
  end
end