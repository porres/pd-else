local test = pd.Class:new():register("test~")

function test:initialize(sel, atoms)
  self.inlets = {SIGNAL}
  self.outlets = {SIGNAL}
  return true
end

function test:dsp(samplerate, blocksize, inchans)
  pd.post(string.format("samplerate: %d, block size: %d, input channels: %s",
    samplerate, blocksize, inchans[1]))
  self.blocksize = blocksize
  self:signal_setmultiout(1, 2)
end

function test:perform(in1)
  out1 = {}
  for i = 1, self.blocksize do
    out1[i] = in1[i + self.blocksize]
    out1[i + self.blocksize] = in1[i]
  end
  return out1
end
