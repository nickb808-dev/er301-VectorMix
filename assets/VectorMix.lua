-- VectorMix.lua — ER-301 unit wrapper for VectorMix v0.3.0
--
-- SIGNAL FLOW
-- ───────────
--   In1 (chain, L[+R in stereo lanes]) ─────────────────┐
--   In2 (branch → ConstantGain fader) ───────────────────┤
--   In3 (branch → ConstantGain fader) ───────────────────┤→ [VectorMix C++] → Out(L/R)
--   In4 (branch → ConstantGain fader) ───────────────────┘
--
--   X, Y move the cursor through the 2-D mix field.
--   Law shapes the blend: −1 circular, 0 linear, +1 gate.
--   Level scales the output.
--
-- CORNER LAYOUT
-- ─────────────
--   In3 ─────────── In4
--    │    X, Y        │
--    │       ●        │
--   In1 ─────────── In2
--
-- v0.3.0
-- ──────
--   • TRUE STEREO in stereo lanes: the chain L/R feed In1/In1R, each corner
--     branch is a STEREO branch (two ConstantGains with tied gains), and the
--     unit outputs an independent L/R mix from ONE shared gain set.
--     Mono lanes behave exactly as before.
--   • In2–In4 are now real level FADERS (BranchMeter over ConstantGain) —
--     the old GainBias dials edited BIAS, i.e. injected DC offset into the
--     corner audio.  Faders default to unity and meter the branch.
--   • Cursor defaults to corner 1 (X = Y = −1): inserting the unit is
--     transparent (chain passes at unity) until you move into the field.
--   • Mix law menu option: amplitude (Σg = 1) or equal power (Σg² = 1).

local app      = app
local Class    = Class or require "Base.Class"
local Unit     = require "Unit"
local GainBias = require "Unit.ViewControl.GainBias"
local BranchMeter    = require "Unit.ViewControl.BranchMeter"
local MenuHeader     = require "Unit.MenuControl.Header"
local OptionControl  = require "Unit.MenuControl.OptionControl"
local XYPad    = require "vectormix.XYPad"
local Encoder  = require "Encoder"

local libvectormix = require "vectormix.libvectormix"

local VectorMix = Class {}
VectorMix:include(Unit)

function VectorMix:init(args)
  args.title    = "Vector Mix"
  args.mnemonic = "VM"
  Unit.init(self, args)
end

-- ── Signal graph ─────────────────────────────────────────────────────────────

-- One corner input: ConstantGain fader(s) → vm inlet(s).
-- Mono lane: one gain, mono branch.  Stereo lane: L/R gains with tied Gain
-- (one fader drives both channels), stereo branch — the MixerUnit pattern.
local function corner(self, vm, name, port, stereo)
  local gL = self:addObject(name, app.ConstantGain())
  gL:setClampInDecibels(-59.9)
  gL:hardSet("Gain", 1.0)
  connect(gL, "Out", vm, port)

  if stereo then
    local gR = self:addObject(name .. "R", app.ConstantGain())
    gR:setClampInDecibels(-59.9)
    gR:hardSet("Gain", 1.0)
    connect(gR, "Out", vm, port .. "R")
    tie(gR, "Gain", gL, "Gain")
    self:addStereoBranch(name, gL, "In", gR, "In", gL, "Out", gR, "Out")
  else
    self:addMonoBranch(name, gL, "In", gL, "Out")
  end
  return gL
end

function VectorMix:onLoadGraph(channelCount)
  local vm = self:addObject("vm", libvectormix.VectorMix())
  local stereo = (channelCount > 1)

  corner(self, vm, "in2", "In2", stereo)
  corner(self, vm, "in3", "In3", stereo)
  corner(self, vm, "in4", "In4", stereo)

  -- ── X / Y — cursor position [−1, +1]; DEFAULT = corner 1 (−1, −1) so a
  --    fresh insert passes the chain at unity (transparent until you move).
  local xParam = self:addObject("xParam", app.GainBias())
  local xRange = self:addObject("xRange", app.MinMax())
  xParam:hardSet("Bias", -1.0)
  connect(xParam, "Out", xRange, "In")
  connect(xParam, "Out", vm,     "X")
  self:addMonoBranch("xMod", xParam, "In", xParam, "Out")

  local yParam = self:addObject("yParam", app.GainBias())
  local yRange = self:addObject("yRange", app.MinMax())
  yParam:hardSet("Bias", -1.0)
  connect(yParam, "Out", yRange, "In")
  connect(yParam, "Out", vm,     "Y")
  self:addMonoBranch("yMod", yParam, "In", yParam, "Out")

  -- ── Law — panning law exponent [−1, +1] ──────────────────────────────────
  local lawParam = self:addObject("lawParam", app.GainBias())
  local lawRange = self:addObject("lawRange", app.MinMax())
  lawParam:hardSet("Bias", 0.0)
  connect(lawParam, "Out", lawRange, "In")
  connect(lawParam, "Out", vm,       "Law")
  self:addMonoBranch("lawMod", lawParam, "In", lawParam, "Out")

  -- ── Level — output gain [0, 2] ────────────────────────────────────────────
  local levelParam = self:addObject("levelParam", app.GainBias())
  local levelRange = self:addObject("levelRange", app.MinMax())
  levelParam:hardSet("Bias", 1.0)
  connect(levelParam, "Out", levelRange, "In")
  connect(levelParam, "Out", vm,         "Level")
  self:addMonoBranch("levelMod", levelParam, "In", levelParam, "Out")

  -- ── Chain routing ──────────────────────────────────────────────────────────
  connect(self, "In1", vm,   "In1")
  connect(vm,   "Out", self, "Out1")
  if stereo then
    connect(self, "In2",  vm,   "In1R")   -- right chain → corner 1 R
    connect(vm,   "OutR", self, "Out2")
  end
end

-- ── Unit menu: mix law option (v0.3.0) ───────────────────────────────────────

function VectorMix:onShowMenu(objects, branches)
  local controls = {}
  local menu = { "optionsHeader", "power" }

  controls.optionsHeader = MenuHeader {
    description = "Vector Mix Options"
  }

  -- amplitude: Σg = 1 (constant amplitude — best for correlated material)
  -- equal power: Σg² = 1 (constant loudness for four unrelated sources;
  --              centre sits at 0.5 per corner instead of 0.25)
  controls.power = OptionControl {
    description = "Mix Law",
    option      = objects.vm:getOption("Power"),
    choices     = { "amplitude", "equal power" },
    descriptionWidth = 2,
  }

  return controls, menu
end

-- ── Encoder views ─────────────────────────────────────────────────────────────

function VectorMix:onLoadViews(objects, branches)
  local controls = {}
  local views = {
    expanded  = {"pad", "in2", "in3", "in4", "x", "y", "law", "level"},
    collapsed = {},
  }

  -- In2–In4 — corner level faders with branch metering (v0.3.0: these were
  -- GainBias dials that edited BIAS = DC offset injection; now true levels).
  controls.in2 = BranchMeter {
    button     = "In2",
    branch     = branches.in2,
    faderParam = objects.in2:getParameter("Gain"),
  }
  controls.in3 = BranchMeter {
    button     = "In3",
    branch     = branches.in3,
    faderParam = objects.in3:getParameter("Gain"),
  }
  controls.in4 = BranchMeter {
    button     = "In4",
    branch     = branches.in4,
    faderParam = objects.in4:getParameter("Gain"),
  }
  self:addToMuteGroup(controls.in2)
  self:addToMuteGroup(controls.in3)
  self:addToMuteGroup(controls.in4)

  local xyMap     = app.LinearDialMap(-1.0, 1.0)
  local xyGainMap = app.LinearDialMap(-2.0, 2.0)
  xyMap:setCoarseRadix(100)
  xyGainMap:setCoarseRadix(100)

  -- Pad — X/Y joystick display (encoder edits X or Y; see XYPad.lua).
  controls.pad = XYPad {
    button  = "xy",
    xParam  = objects.xParam,
    yParam  = objects.yParam,
    xBranch = branches.xMod,
    yBranch = branches.yMod,
    xyMap   = xyMap,
  }

  controls.x = GainBias {
    button      = "x",
    description = "X Position",
    branch      = branches.xMod,
    gainbias    = objects.xParam,
    range       = objects.xRange,
    biasMap     = xyMap,
    initialBias = -1.0,
    gainMap     = xyGainMap,
  }

  controls.y = GainBias {
    button      = "y",
    description = "Y Position",
    branch      = branches.yMod,
    gainbias    = objects.yParam,
    range       = objects.yRange,
    biasMap     = xyMap,
    initialBias = -1.0,
    gainMap     = xyGainMap,
  }

  controls.law = GainBias {
    button      = "law",
    description = "Panning Law",
    branch      = branches.lawMod,
    gainbias    = objects.lawParam,
    range       = objects.lawRange,
    biasMap     = xyMap,
    initialBias = 0.0,
    gainMap     = xyGainMap,
  }

  controls.level = GainBias {
    button      = "level",
    description = "Output Level",
    branch      = branches.levelMod,
    gainbias    = objects.levelParam,
    range       = objects.levelRange,
    biasMap     = Encoder.getMap("[0,2]"),
    initialBias = 1.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }

  return controls, views
end

return VectorMix
