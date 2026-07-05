-- XYPad.lua — X/Y cursor ViewControl for VectorMix v0.2.0
--
-- A custom EncoderControl that shows a VectorPad graphic in the main 42×64
-- slot and provides encoder-based editing of both X and Y parameters.
--
-- INTERACTION
-- ───────────
--   When focused, the encoder moves X (sub-button 2 selected) or Y (sub-button
--   3 selected).  The dial-press toggles Coarse / Fine step size.  Sub-button 1
--   opens the X CV modulation branch.
--
-- SUB-DISPLAY (128 × 64 px)
-- ──────────────────────────
--   ┌───────────────────────────────────────────────────────────────────────┐
--   │   [cv]          [x]          [y]      ← sub-buttons 1, 2, 3          │
--   │                                                                       │
--   │              x           y            ← parameter labels             │
--   │           [ 0.00 ]   [ 0.00 ]         ← readouts                    │
--   └───────────────────────────────────────────────────────────────────────┘
--
-- USAGE FROM VectorMix.lua
-- ─────────────────────────
--   local XYPad = require "vectormix.XYPad"
--
--   controls.pad = XYPad {
--     button  = "xy",
--     xParam  = objects.xParam,    -- app.GainBias holding the X value
--     yParam  = objects.yParam,    -- app.GainBias holding the Y value
--     xBranch = branches.xMod,     -- MonoBranch for X CV patching
--     yBranch = branches.yMod,     -- MonoBranch for Y CV patching (optional)
--     xyMap   = xyMap,             -- LinearDialMap(-1, 1) for the readouts
--   }

local app     = app
local Class   = Class or require "Base.Class"
local Base    = require "Unit.ViewControl.EncoderControl"
local Encoder = require "Encoder"

local libvectormix = require "vectormix.libvectormix"

-- ── Layout constants (match the ER-301 sub-display grid) ─────────────────────
local ply      = app.SECTION_PLY      -- 42 px — main slot width
local line1    = app.GRID5_LINE1      -- top grid line
local line4    = app.GRID5_LINE4
local center1  = app.GRID5_CENTER1   -- top row centre
local center4  = app.GRID5_CENTER4   -- bottom row centre
local col2     = app.BUTTON2_CENTER   -- 63 px — left readout column
local col3     = app.BUTTON3_CENTER   -- 106 px — right readout column

-- ── XYPad class ──────────────────────────────────────────────────────────────

local XYPad = Class { type = "XYPad", canMove = true, canEdit = false }
XYPad:include(Base)

function XYPad:init(args)
  local button  = args.button  or app.logError("%s.init: button missing",  self)
  local xParam  = args.xParam  or app.logError("%s.init: xParam missing",  self)
  local yParam  = args.yParam  or app.logError("%s.init: yParam missing",  self)

  Base.init(self, button)
  self:setClassName("vectormix.XYPad")

  -- ── Wire up the underlying bias parameters ──────────────────────────────
  local xBiasParam = xParam:getParameter("Bias")
  local yBiasParam = yParam:getParameter("Bias")
  xBiasParam:enableSerialization()
  yBiasParam:enableSerialization()

  -- ── Main control graphic: VectorPad ─────────────────────────────────────
  -- VectorPad tracks the actual GainBias output (CV + bias) via the outlet so
  -- that patched CV moves the dot, not just the encoder.  The parameter wiring
  -- is kept as a fallback for when no outlet is set.
  local pad = libvectormix.VectorPad(0, 0, ply, 64)
  pad:setXParameter(xBiasParam)
  pad:setYParameter(yBiasParam)
  pad:setXOutlet(args.xParam:getOutput("Out"))
  pad:setYOutlet(args.yParam:getOutput("Out"))
  self:setMainCursorController(pad)
  self:setControlGraphic(pad)
  self:addSpotDescriptor { center = 0.5 * ply }
  self.pad = pad

  -- ── Sub display ──────────────────────────────────────────────────────────
  self.subGraphic = app.Graphic(0, 0, 128, 64)

  -- X readout
  local xMap = args.xyMap or Encoder.getMap("[-1,1]")
  local xReadout = app.Readout(0, 0, ply, 10)
  xReadout:setParameter(xBiasParam)
  xReadout:setMap(xMap)
  xReadout:setCenter(col2, center4)
  self.xReadout = xReadout
  self.subGraphic:addChild(xReadout)

  -- Y readout
  local yMap = args.xyMap or Encoder.getMap("[-1,1]")
  local yReadout = app.Readout(0, 0, ply, 10)
  yReadout:setParameter(yBiasParam)
  yReadout:setMap(yMap)
  yReadout:setCenter(col3, center4)
  self.yReadout = yReadout
  self.subGraphic:addChild(yReadout)

  -- Labels above the readouts
  local xLabel = app.Label("x", 10)
  xLabel:fitToText(2)
  xLabel:setCenter(col2, center1)
  self.subGraphic:addChild(xLabel)

  local yLabel = app.Label("y", 10)
  yLabel:fitToText(2)
  yLabel:setCenter(col3, center1)
  self.subGraphic:addChild(yLabel)

  -- Sub-buttons:
  --   1 = open X CV mod branch
  --   2 = focus encoder on X
  --   3 = focus encoder on Y
  self.subGraphic:addChild(app.SubButton("cv",  1))
  self.subGraphic:addChild(app.SubButton("x",   2))
  self.subGraphic:addChild(app.SubButton("y",   3))

  -- Store branches for sub-button 1 navigation
  self.xBranch = args.xBranch
  self.yBranch = args.yBranch

  -- Default: encoder edits X
  self.focused = "x"
  self:setFocusedReadout(self.xReadout)
end

-- ── Focus helpers ─────────────────────────────────────────────────────────────

function XYPad:setFocusedReadout(readout)
  if readout then readout:save() end
  self.focusedReadout = readout
  self:setSubCursorController(readout)
  Encoder.set(self.encoderState)
end

-- ── EncoderControl overrides ──────────────────────────────────────────────────

function XYPad:spotReleased(spot, shifted)
  if Base.spotReleased(self, spot, shifted) then
    -- Re-focus on X when the spot is first activated
    self.focused = "x"
    self:setFocusedReadout(self.xReadout)
    return true
  end
  return false
end

function XYPad:encoder(change, shifted)
  self.focusedReadout:encoder(change, shifted, self.encoderState == Encoder.Fine)
  return true
end

function XYPad:zeroPressed()
  self.focusedReadout:zero()
  return true
end

function XYPad:cancelReleased(shifted)
  if shifted then return false end
  self.focusedReadout:restore()
  return true
end

function XYPad:onFocused()
  self.focusedReadout:save()
end

-- ── Sub-button handler ────────────────────────────────────────────────────────

function XYPad:subReleased(i, shifted)
  if i == 1 then
    -- Open X mod branch for CV patching.
    local branch = self.xBranch
    if branch then
      self:unfocus()
      branch:show()
    end
  elseif i == 2 then
    -- Focus encoder on X; double-tap opens keyboard entry.
    if self:hasFocus("encoder") and self.focused == "x" then
      self:doDirectEntry(self.xReadout, "X position [-1, +1]", "X updated.")
    else
      self:focus()
      self.focused = "x"
      self:setFocusedReadout(self.xReadout)
    end
  elseif i == 3 then
    -- Focus encoder on Y; double-tap opens keyboard entry.
    if self:hasFocus("encoder") and self.focused == "y" then
      self:doDirectEntry(self.yReadout, "Y position [-1, +1]", "Y updated.")
    else
      self:focus()
      self.focused = "y"
      self:setFocusedReadout(self.yReadout)
    end
  end
  return true
end

-- ── Direct-entry keyboard ─────────────────────────────────────────────────────

function XYPad:doDirectEntry(readout, message, commitMessage)
  local Decimal = require "Keyboard.Decimal"
  local kb = Decimal {
    message       = message,
    commitMessage = commitMessage,
    initialValue  = readout:getValueInUnits(),
  }
  local task = function(value)
    if value then
      readout:save()
      readout:setValueInUnits(value)
      self:unfocus()
    end
  end
  kb:subscribe("done",   task)
  kb:subscribe("commit", task)
  kb:show()
end

-- ── Serialization ─────────────────────────────────────────────────────────────

function XYPad:serialize()
  local t = Base.serialize(self)
  t.focused = self.focused
  return t
end

function XYPad:deserialize(t)
  Base.deserialize(self, t)
  if t.focused == "y" then
    self.focused = "y"
    self:setFocusedReadout(self.yReadout)
  end
end

return XYPad
