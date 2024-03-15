local pdlua_flames = {}
local scope3d = pd.Class:new():register("scope3d~")

function scope3d:initialize(name, args)
  self.inlets = {SIGNAL, SIGNAL, SIGNAL}
  local methods =
  {
    { name = "rotate",      defaults = { 0, 0 } },
    { name = "hrotate" },
    { name = "vrotate" },

    { name = "dim",         defaults = { 140, 140 } },

    { name = "width",       defaults = { 1 } },
    { name = "clip",        defaults = { 1 } },
    { name = "grid",        defaults = { 1 } },
    { name = "drag",        defaults = { 1 } },
    { name = "zoom",        defaults = { 1 } },
    { name = "perspective", defaults = { 1 } },
    { name = "rate",        defaults = { 50 } },

    { name = "list",        defaults = { 8, 256 } },
    { name = "nsamples" },
    { name = "nlines" },

    { name = "fgcolor",     defaults = { 30, 30, 30 } },
    { name = "bgcolor",     defaults = { 190, 190, 190 } },
    { name = "gridcolor",   defaults = { 160, 160, 160 } },
    { name = "receive",     defaults = { "empty" } },
  }
  self.cameraDistance = 6
  self.rotationAngleX, self.rotationAngleY = 0, 0
  self.rotationStartAngleX, self.rotationStartAngleY = 0, 0
  self.gridLines = self:create_grid(-1, 1, 0.25)

  self.pd_env = { ignorewarnings = true }
  pdlua_flames:init_pd_methods(self, name, methods, args)
  self:set_size(table.unpack(self.pd_methods.dim.val))
  self:reset_buffer()
  return true
end

function scope3d:create_grid(minVal, maxVal, step)
  local grid = {}
  for i = minVal, maxVal, step do
    table.insert(grid, {{i, 0, minVal}, {i, 0, maxVal}})
    table.insert(grid, {{minVal, 0, i}, {maxVal, 0, i}})
  end
  return grid
end

function scope3d:reset_buffer()
  self.bufferIndex = 1
  self.sampleIndex = 1
  self.signal = {}
  self.rotatedSignal = {}
  -- prefill ring buffer
  for i = 1, self.nlines do 
    self.signal[i] = {0, 0, 0}
    self.rotatedSignal[i] = {0, 0, 0}
  end
end

function scope3d:postinitialize()
  self.clock = pd.Clock:new():register(self, "tick")
  self:pd_rate(self.pd_methods.rate.val)
end

function scope3d:tick()
  self:repaint()
  self.clock:delay(self.framedelay)
end

function scope3d:finalize()
  self.clock:destruct()
  if self.recv then self.recv:destruct() end
end

function scope3d:pd_rate(x)
  if type(x[1]) == "number" then
    self.framedelay = math.max(8, 1000 / x[1])
  end
  if self.clock then
    self.clock:unset() 
    self.clock:delay(self.framedelay)
  end
end

function scope3d:pd_list(x)
  self.nsamples = math.max(2, math.floor(x[1])) - 1 or 8
  self.nlines = math.min(1024, math.max(2, math.floor(x[2]))) or 256
  self:reset_buffer()
end

function scope3d:pd_nsamples(x)
  local allAtoms = self.pd_methods.list.val
  allAtoms[1] = x[1]
  self:handle_pd_message('list', allAtoms)
end

function scope3d:pd_nlines(x)
  local allAtoms = self.pd_methods.list.val
  allAtoms[2] = x[1]
  self:handle_pd_message('list', allAtoms)
end

function scope3d:pd_rotate(x)
  self.rotationAngleY, self.rotationAngleX = x[1], x[2]
  self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
end

function scope3d:pd_hrotate(x)
  local allAtoms = {self.rotationAngleY, self.rotationAngleX}
  allAtoms[1] = x[1]
  self:handle_pd_message('rotate', allAtoms)
end

function scope3d:pd_vrotate(x)
  local allAtoms = {self.rotationAngleY, self.rotationAngleX}
  allAtoms[2] = x[1]
  self:handle_pd_message('rotate', allAtoms)
end

function scope3d:pd_dim(x)
  self:set_size(x[1], x[2])
end

function scope3d:dsp(sr, bs)
    self.blocksize = bs
end

function scope3d:mouse_down(x, y)
  if self.pd_methods.drag.val[1] == 1 then self.dragStartX, self.dragStartY = x, y end
end

function scope3d:mouse_up(x, y)
  if self.pd_methods.drag.val[1] == 1  then
    self.rotationStartAngleX, self.rotationStartAngleY = self.rotationAngleX, self.rotationAngleY
  end
end

function scope3d:mouse_drag(x, y)
  if self.pd_methods.drag.val[1] == 1 then 
    self.rotationAngleY = self.rotationStartAngleY + ((x-self.dragStartX) / 2)
    self.rotationAngleX = self.rotationStartAngleX + ((y-self.dragStartY) / 2)
  end
end

function scope3d:perform(in1, in2, in3)
  while self.sampleIndex <= self.blocksize do
    -- circular buffer
    self.signal[self.bufferIndex] = {in1[self.sampleIndex], in2[self.sampleIndex], in3[self.sampleIndex]}
    self.bufferIndex = (self.bufferIndex % self.nlines) + 1
    self.sampleIndex = self.sampleIndex + self.nsamples
  end
  self.sampleIndex = self.sampleIndex - self.blocksize
end

function scope3d:clipLine(x1, y1, x2, y2)
    local xmin, ymin, xmax, ymax = 1, 1, self.pd_methods.dim.val[1]-1, self.pd_methods.dim.val[2]-1

    -- Check if a point is inside the bounding box
    local function isInside(x, y)
        return x >= xmin and x <= xmax and y >= ymin and y <= ymax
    end

    local function computeOutCode(x, y)
        local code = 0
        if x < xmin then -- to the left of clip window
            code = code | 1
        elseif x > xmax then -- to the right of clip window
            code = code | 2
        end
        if y < ymin then -- below the clip window
            code = code | 4
        elseif y > ymax then -- above the clip window
            code = code | 8
        end
        return code
    end

    local function clipPoint(x, y, outcode)
        local dx = x2 - x1
        local dy = y2 - y1

        if outcode & 8 ~= 0 then -- point is above the clip window
            x = x1 + dx * (ymax - y) / dy
            y = ymax
        elseif outcode & 4 ~= 0 then -- point is below the clip window
            x = x1 + dx * (ymin - y) / dy
            y = ymin
        elseif outcode & 2 ~= 0 then -- point is to the right of clip window
            y = y1 + dy * (xmax - x) / dx
            x = xmax
        elseif outcode & 1 ~= 0 then -- point is to the left of clip window
            y = y1 + dy * (xmin - x) / dx
            x = xmin
        end
        return x, y
    end

    local outcode1 = computeOutCode(x1, y1)
    local outcode2 = computeOutCode(x2, y2)
    local accept = false

    while true do
        if outcode1 == 0 and outcode2 == 0 then -- Both points inside
            accept = true
            break
        elseif (outcode1 & outcode2) ~= 0 then -- Both points share an outside zone
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
  g:set_color(table.unpack(self.pd_methods.bgcolor.val))
  g:fill_all()

  -- draw ground grid
  if self.pd_methods.grid.val[1] == 1 then
    g:set_color(table.unpack(self.pd_methods.gridcolor.val))
    for i = 1, #self.gridLines do
      local lineFrom, lineTo = table.unpack(self.gridLines[i])
      
      -- apply rotation to grid lines
      lineFrom = self:rotate_y(lineFrom, self.rotationAngleY)
      lineFrom = self:rotate_x(lineFrom, self.rotationAngleX)
      lineTo   = self:rotate_y(lineTo  , self.rotationAngleY)
      lineTo   = self:rotate_x(lineTo  , self.rotationAngleX)

      local startX, startY = self:projectVertex(lineFrom, self.pd_methods.zoom.val[1])
      local   endX,   endY = self:projectVertex(  lineTo, self.pd_methods.zoom.val[1])
      if self.pd_methods.clip.val[1] == 1 then 
        startX, startY, endX, endY = self:clipLine(startX, startY, endX, endY)
      end
      if startX then g:draw_line(startX, startY, endX, endY, 1) end
      -- if lineFrom[3] > -self.cameraDistance and lineTo[3] > -self.cameraDistance then
      -- end
    end
  end

  for i = 1, self.nlines do
    local offsetIndex = (i + self.bufferIndex-2) % self.nlines + 1
    local rotatedVertex = self:rotate_y(self.signal[offsetIndex], self.rotationAngleY)
    self.rotatedSignal[i] = self:rotate_x(rotatedVertex, self.rotationAngleX)
  end


  g:set_color(table.unpack(self.pd_methods.fgcolor.val))
  local startX, startY = self:projectVertex(self.rotatedSignal[1], self.pd_methods.zoom.val[1])
  for i = 2, self.nlines do
    local endX, endY = self:projectVertex(self.rotatedSignal[i], self.pd_methods.zoom.val[1])
    local prevX, prevY = endX, endY
    if self.pd_methods.clip.val[1] == 1 then
      startX, startY, endX, endY = self:clipLine(startX, startY, endX, endY)
    end
    if startX then g:draw_line(startX, startY, endX, endY, self.pd_methods.width.val[1]) end
    startX, startY = prevX, prevY
  end
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
  local cosTheta = math.cos(-angle * math.pi / 180)
  local sinTheta = math.sin(-angle * math.pi / 180)
  local newY = y * cosTheta - z * sinTheta
  local newZ = y * sinTheta + z * cosTheta
  return {x, newY, newZ}
end

function scope3d:projectVertex(vertex)
  local maxDim = math.max(self.pd_methods.dim.val[1] - 2, self.pd_methods.dim.val[2] - 2)
  local scale = self.cameraDistance / (self.cameraDistance + vertex[3] * self.pd_methods.perspective.val[1])
  local screenX = self.pd_methods.dim.val[1] / 2 + 0.5 + (vertex[1] * scale * self.pd_methods.zoom.val[1] * maxDim * 0.5)
  local screenY = self.pd_methods.dim.val[2] / 2 + 0.5 - (vertex[2] * scale * self.pd_methods.zoom.val[1] * maxDim * 0.5)
  return screenX, screenY
end

function scope3d:receive(sel, atoms)
  self:handle_pd_message(sel, atoms)
end

function scope3d:in_n(n, sel, atoms)
  self:handle_pd_message(sel, atoms, n)
end

function scope3d:pd_receive(x)
  if self.recv then self.recv:destruct() end
  if x[1] then
    self.recv = pd.Receive:new():register(self, tostring(x[1]), "receive")
  end
end

-------------------------------------------------------------------------------
--                                       ██  
--                             ▓▓      ██    
--                                 ████▒▒██  
--                               ████▓▓████  
--                             ██▓▓▓▓▒▒██    
--                             ██▓▓▓▓▓▓██    
--                           ████▒▒▓▓▓▓████  
--                           ██▒▒▓▓░░▓▓▒▒▓▓██
--                           ██████▓▓░░▓▓░░██
--                             ██░░░░▒▒░░████
--                             ██░░  ░░░░██  
--                               ░░    ░░
--
-- initializing methods and state (with defaults or creation arguments)
-- creates:
--   * self.pd_args for saving state
--   * self.pd_env for object name (used in error log) and more config
--   * self.pd_methods for look-ups of:
--     1. corresponding function if defined
--     2. state index for saving method states
--     3. method's argument count
--     4. current values
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

function pdlua_flames:parse_atoms(atoms)
  local kwargs = {}
  local args = {}
  local collectKey = nil
  for _, atom in ipairs(atoms) do
    if type(atom) ~= 'number' and string.sub(atom, 1, 1) == '-' then
      -- start collecting values for a new key if key detected
      collectKey = string.sub(atom, 2)
      kwargs[collectKey] = {}
    elseif collectKey then
      -- if collecting values for a key, add atom to current key's table
      table.insert(kwargs[collectKey], atom)
    else
      -- otherwise treat as a positional argument
      table.insert(args, atom)
    end
  end
  return kwargs, args
end

-- handle messages and update state
-- function gets mixed into object's class for use as self:handle_pd_messages
function pdlua_flames:handle_pd_message(sel, atoms, n)
  if self.pd_methods[sel] then
    local startIndex = self.pd_methods[sel].index
    local valueCount = self.pd_methods[sel].arg_count
    -- call function an save result (if returned)
    local returnValues = self.pd_methods[sel].func and self.pd_methods[sel].func(self, atoms)
    local values = {}
    -- clip incoming values to arg_count
    if startIndex and valueCount then
      for i, atom in ipairs(returnValues or atoms) do
        if i > valueCount then break end
        table.insert(values, atom)
        self.pd_args[startIndex + i-1] = atom
      end
    end
    self.pd_methods[sel].val = values
    -- update object state
    self:set_args(self.pd_args)
  else
    local baseMessage = self.pd_env.name .. ': no method for \'' .. sel .. '\''
    local inletMessage = n and ' on inlet ' .. string.format('%d', n) or ''
    self:error(baseMessage .. inletMessage)
  end
end