--[[ 
--  ltextfile-drip
--  Output a text file as a sequence of symbols (sentences)
--  author Martin Peach 20120913
--]]

-- Pd class

local LTextFileDrip = pd.Class:new():register("ltextfile-drip")
local file ourTextFile = nil
local words = {}
local wordIndex = 0
local playbackIndex = 0
local ourSentence, ourRemainingLine = nil
local ourLine = nil

function LTextFileDrip:initialize(name, atoms)
--pd.post("LTextFileDrip:initialize-> name is " .. name);
--pd.post("number of atoms is ".. #atoms)
--for i,v in ipairs(atoms) do
--  pd.post(i .. " = " .. v)
--end
  if #atoms > 0 then self:openOurTextFile(atoms[1]) end
  if ourTextFile == nil then pd.post("LTextFileDrip:initialize: unable to open " .. atoms[1]) end
  self.inlets = 1
  self.outlets = 3
  return true
end

-- LTextFileDrip:openOurTextFile: open a text file using name as a path
function LTextFileDrip:openOurTextFile(name)
  if ourTextFile ~= nil then
--pd.post("LTextFileDrip:openOurTextFile: closing old file")
    ourTextFile:close()
    ourTextFile = nil
  end
--pd.post("LTextFileDrip:openOurTextFile ( " .. name .. " )")
  ourTextFile = io.open(name)
  if ourTextFile == nil then
    pd.post("LTextFileDrip:openOurTextFile: Unable to open " .. name)
  end
  ourRemainingLine = nil
end

function LTextFileDrip:rewindOurTextFile()
--  pd.post("LTextFileDrip:rewind")
  if ourTextFile == nil then
    pd.post("LTextFileDrip:rewindOurTextFile: no open file")
  else
    ourTextFile:seek("set") --sets the position to the beginning of the file (and returns 0)
    wordIndex = 0
    playbackIndex = 0
  end
end

-- LTextFileDrip:drip: accumulate a line of words from ourTextFile and output them as symbols, one per bang
function LTextFileDrip:drip()
  local ourPunc
  local repeatCount = 0

  if ourTextFile == nil then
    pd.post("LTextFileDrip:drip: no open file")
    return
  end
--  if wordIndex == 0 then -- read another line
  repeat
    repeatCount = repeatCount + 1
--pd.post("repeat number " .. repeatCount)
    if ourRemainingLine == nil then
--pd.post ("1> ourRemainingLine is nil")
      ourLine = ourTextFile:read()
      if ourLine == nil then
        self:outlet(3, "bang", {}) -- end of file
        return
      end
--pd.post ("1> ourLine is [" .. ourLine .. "] len: " .. ourLine:len())
    -- strip CR LF
      s,e = ourLine:find("[\n\r]")
      if s then
--pd.post("1> CR or LF  at " .. s)
        ourLine = ourLine:sub(1,s-1) .. " "
      else
        ourLine = ourLine .. " "
      end    
    elseif ourRemainingLine:len() == 0 then -- read another line
--pd.post ("2> ourRemainingLine length 0")
      ourLine = ourTextFile:read()
      if ourLine == nil then
        self:outlet(3, "bang", {}) -- end of file
        return
      end
--pd.post ("2> ourLine is [" .. ourLine .. "] len: " .. ourLine:len())
      s,e = ourLine:find("[\n\r]")
      if s then
--pd.post("2> CR or LF at " .. s)
        ourLine = ourLine:sub(1,s-1) .. " "
      else
        ourLine = ourLine .. " "
      end
--pd.post("Not setting ourLine from ourRemainingLine, whose length is precisely " .. ourRemainingLine:len())
    else
      cstart,cend = ourRemainingLine:find("[,.;!?:]")
      if cstart == nil then -- no punctuation in remainingLine, add next line from ourTextFile
--pd.post("3> Adding a new line to ourRemainingLine")
        ourLine = ourTextFile:read()
        if ourLine == nil then -- no more lines
          self:outlet(3, "bang", {}) -- end of file
          return
        end
--pd.post ("3> ourLine is [" .. ourLine .. "] len: " .. ourLine:len())
        s,e = ourLine:find("[\n\r]")
        if s then
--pd.post("3> CR or LF at " .. s)
          ourLine = ourLine:sub(1,s-1) .. " "
        else
          ourLine = ourLine .. " "
        end
        --ourLine = ourLine:gsub("[\n\r]", " ")
        if (ourLine:len() == 1) then -- an empty line
--pd.post("BLANK LINE (1)")
          ourLine = "." -- replace blank line with punctuation to force a chop
        end
        ourLine = ourRemainingLine .. ourLine
      else -- remainingLine has punctuation
--pd.post("4> ourRemainingLine contains punctuation")
        ourLine = ourRemainingLine
      end
      ourRemainingLine = nil
    end -- if ourRemainingLine == nil
    ourPunc = nil
    if ourLine ~= nil then -- line has content
      if ourLine:len() > 0 then
        wordIndex = 0
--pd.post("LTextFileDrip:drip: <" .. ourLine .. ">")
--pd.post("LTextFileDrip:drip: len" .. ourLine:len())
--pd.post("LTextFileDrip:drip: 1st char" .. ourLine:byte(1))
        if ((ourLine:len() == 1) and (ourLine:byte(1) == 13)) then -- a blank line
--pd.post("BLANK LINE (2)")
          ourSentence = ourRemainingLine
          ourRemainingLine = nil
        else -- find a comma, period, semicolon, exclamation or question mark
          cstart,cend = ourLine:find("[,.;!?:]")
          if cstart ~= nil then -- take the line before the mark
--pd.post("LTextFileDrip:drip: cstart " .. string.format ('%d', cstart) .. " cend " .. string.format('%d', cend))
            ourPunc = ourLine:sub(cend, cend)
--pd.post("LTextFileDrip:drip: punctuation: " .. ourPunc)
            ourSentence = ourLine:sub(1, cend) -- leave punctuation
            if ourRemainingLine ~= nil then        
              ourSentence = ourRemainingLine .. ourSentence
            end
            ourRemainingLine = ourLine:sub(cend+1)
--pd.post("LTextFileDrip:drip punctuation found: ourSentence is " .. ourSentence)
--pd.post("LTextFileDrip:drip punctuation found: ourRemainingLine is " .. ourRemainingLine)
          else -- no mark, take the whole line
            if ourRemainingLine ~= nil then         
              ourRemainingLine = ourRemainingLine .. ourLine
--pd.post("LTextFileDrip:drip more to come: ourRemainingLine is " .. ourRemainingLine)
--for c = 1, ourRemainingLine:len() do pd.post("_" .. ourRemainingLine:byte(c) .. "_") end
            else
              ourRemainingLine = ourLine
--pd.post("LTextFileDrip:drip first: ourRemainingLine is " .. ourRemainingLine)
            end
          end -- if cstart ~= nil
        end -- if ((ourLine:len() == 1) and (ourLine:byte(1) == 13))
      end -- if ourLine:len() > 0
      if ourSentence ~= nil then
        if ourSentence:gmatch("%w+") then
          if ((ourSentence:len() == 1) and (ourSentence:byte(1) <= 32)) then
--pd.post("BLANK LINE (3)")
            wordIndex = 0 -- skip blank line
          else
           wordIndex = 1
          end
--        for w in ourSentence:gmatch("%w+") do
--          wordIndex = wordIndex + 1
--          words[wordIndex] = w
--pd.post("wordIndex " .. wordIndex .. " for length " .. ourSentence:len() .. " starts with " .. ourSentence:byte(1))
        end
      end
      playbackIndex = 1
    else -- bang through outlet 3 serves as end of file notification
      self:outlet(3, "bang", {})
    end -- if ourLine ~= nil
--  end
  until ((wordIndex ~= 0) or (ourLine == nil))
  if ourPunc ~= nil then
    self:outlet(2, "symbol", {ourPunc}) -- output punctuation
  end
  if wordIndex > 0 then -- output sentence as a single symbol
    if ourSentence ~= nil then
      len = ourSentence:len()
--pd.post ("ourSentence:len() is " .. len)
      if len > 0 then
--pd.post("ourSentence is not zero-length")
--pd.post("ourSentence [" .. ourSentence .. "]")
--pd.post("starts with " .. ourSentence:byte(1))
        self:outlet(1, "symbol", {ourSentence}) -- output entire line
      else
--pd.post("ourSentence is zero-length")
        self:outlet(1, "bang", {}) -- an empty line
      end
    end
    wordIndex = 0 -- no more words
    ourSentence = nil -- no more sentence
  end
end

function LTextFileDrip:in_1(sel, atoms) -- anything
--  pd.post("LTextFileDrip:in-> sel is " .. sel);
--  pd.post("number of atoms is ".. #atoms)
--  for i,v in ipairs(atoms) do
--    pd.post(i .. " = " .. v)
--  end
  
  if sel == "bang" then -- output the next sentence
    self:drip()
  elseif sel == "open" then -- open a file
    self:openOurTextFile(atoms[1])    
  elseif sel == "rewind" then -- rewind the file
    self:rewindOurTextFile()    
  elseif sel == "symbol" then -- assume it's a file name
    self:openOurTextFile(atoms[1])
  elseif #atoms == 0 then -- a selector
    self:error("LTextFileDrip:in_1: input not known")
  else -- reject non-symbols
    self:error("LTextFileDrip:in_1: input not a symbol or bang")
  end
end
-- end of ltextfile-drip

