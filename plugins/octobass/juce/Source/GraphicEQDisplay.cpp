#include "GraphicEQDisplay.h"

GraphicEQDisplay::GraphicEQDisplay()
{
  setTitle("Graphic EQ");
  setWantsKeyboardFocus(true);
  addAndMakeVisible(spectrumDisplay_);
  spectrumDisplay_.setInterceptsMouseClicks(false, false);
}

void GraphicEQDisplay::setNode(int slot, bool active, float freqHz, float gainDb)
{
  if (slot < 0 || slot >= kNumNodes)
    return;

  auto& node = nodes_[static_cast<size_t>(slot)];
  freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
  gainDb = juce::jlimit(kMinGainDb, kMaxGainDb, gainDb);

  if (node.active != active || std::fabs(node.freqHz - freqHz) > 0.01f ||
      std::fabs(node.gainDb - gainDb) > 0.001f)
  {
    node.active = active;
    node.freqHz = freqHz;
    node.gainDb = gainDb;
    repaint();
  }
}

void GraphicEQDisplay::setLowCut(bool active, float freqHz)
{
  freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
  if (lowCut_.active != active || std::fabs(lowCut_.freqHz - freqHz) > 0.01f)
  {
    lowCut_.active = active;
    lowCut_.freqHz = freqHz;
    repaint();
  }
}

void GraphicEQDisplay::setHighCut(bool active, float freqHz)
{
  freqHz = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
  if (highCut_.active != active || std::fabs(highCut_.freqHz - freqHz) > 0.01f)
  {
    highCut_.active = active;
    highCut_.freqHz = freqHz;
    repaint();
  }
}

void GraphicEQDisplay::paintOverChildren(juce::Graphics& g)
{
  auto barArea = getBarArea();
  if (barArea.getWidth() <= 0 || barArea.getHeight() <= 0)
    return;

  float areaTop = static_cast<float>(barArea.getY());
  float areaH = static_cast<float>(barArea.getHeight());
  float areaLeft = static_cast<float>(barArea.getX());
  float areaW = static_cast<float>(barArea.getWidth());

  // Center line at 0dB gain
  float centerY = gainToY(0.0f, areaTop, areaH);
  g.setColour(kLCDInkColour.withAlpha(0.3f));
  g.drawHorizontalLine(static_cast<int>(centerY), areaLeft, areaLeft + areaW);

  bool anyActive = anyFilterActive();

  // Draw actual magnitude response curve
  juce::Path eqPath;
  constexpr int kCurvePoints = 200;

  for (int p = 0; p <= kCurvePoints; ++p)
  {
    float t = static_cast<float>(p) / static_cast<float>(kCurvePoints);
    float x = areaLeft + t * areaW;

    float responseDb = 0.0f;
    if (anyActive)
    {
      responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(normXToFreq(t)));
    }

    float y = gainToY(responseDb, areaTop, areaH);

    if (p == 0)
      eqPath.startNewSubPath(x, y);
    else
      eqPath.lineTo(x, y);
  }

  g.setColour(kLCDInkColour);
  g.strokePath(eqPath, juce::PathStrokeType(2.0f));

  // Ghost dot: hover hint on the curve where a node would be created
  if (ghostVisible_ && dragHandle_ == kNoHandle && hoverHandle_ == kNoHandle)
  {
    constexpr float kGhostRadius = 4.0f;
    g.setColour(kLCDInkColour.withAlpha(0.35f));
    g.fillEllipse(ghostPos_.x - kGhostRadius, ghostPos_.y - kGhostRadius, kGhostRadius * 2.0f,
                  kGhostRadius * 2.0f);
  }

  // Draw a dot for each active node and cut handle
  for (int handle = 0; handle < kNumNodes + 2; ++handle)
  {
    if (!handleActive(handle))
      continue;

    auto pt = handlePixel(handle, barArea);
    float radius = (handle == dragHandle_ || handle == hoverHandle_) ? 5.0f : 4.0f;
    g.setColour(kLCDInkColour);
    g.fillEllipse(pt.x - radius, pt.y - radius, radius * 2.0f, radius * 2.0f);

    // Visible focus indicator for keyboard interaction (WCAG 2.4.7)
    if (handle == focusedHandle_ && focusRingVisible_ && hasKeyboardFocus(true))
    {
      constexpr float kFocusRingGap = 3.0f;
      float ringRadius = radius + kFocusRingGap;
      g.drawEllipse(pt.x - ringRadius, pt.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f,
                    1.5f);
    }
  }

  // Tooltip for the hovered or dragged handle
  int tooltipHandle = (dragHandle_ != kNoHandle) ? dragHandle_ : hoverHandle_;
  if (tooltipHandle != kNoHandle && handleActive(tooltipHandle))
  {
    juce::String text;
    if (tooltipHandle == kLowCutHandle)
      text = "LO CUT";
    else if (tooltipHandle == kHighCutHandle)
      text = "HI CUT";
    else
    {
      float gain = nodes_[static_cast<size_t>(tooltipHandle)].gainDb;
      text = ((gain >= 0.0f) ? "+" : "") + juce::String(gain, 1) + " dB";
    }

    constexpr float kTooltipFontH = 9.0f;
    constexpr float kTooltipPadX = 4.0f;
    constexpr float kTooltipPadY = 2.0f;
    constexpr float kTooltipCorner = 3.0f;

    juce::Font tooltipFont =
        typeface_ != nullptr
            ? juce::Font(juce::FontOptions().withTypeface(typeface_).withHeight(kTooltipFontH))
            : juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                           kTooltipFontH, juce::Font::plain));
    g.setFont(tooltipFont);

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(tooltipFont, text, 0.0f, 0.0f);
    float textW = glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth();
    float boxW = textW + kTooltipPadX * 2.0f;
    float boxH = kTooltipFontH + kTooltipPadY * 2.0f;

    auto pt = handlePixel(tooltipHandle, barArea);
    float boxX = pt.x + 6.0f;
    float boxY = pt.y - boxH - 4.0f;

    // Flip to left side if near right edge
    if (boxX + boxW > areaLeft + areaW)
      boxX = pt.x - boxW - 6.0f;
    // Flip below if near top edge
    if (boxY < areaTop)
      boxY = pt.y + 6.0f;

    auto boxRect = juce::Rectangle<float>(boxX, boxY, boxW, boxH);
    g.setColour(kLCDInkColour);
    g.fillRoundedRectangle(boxRect, kTooltipCorner);

    g.setColour(kLCDBacklightColour);
    g.drawText(text, boxRect.toNearestInt(), juce::Justification::centred, false);
  }
}

void GraphicEQDisplay::mouseMove(const juce::MouseEvent& e)
{
  auto barArea = getBarArea();
  if (barArea.getWidth() <= 0 || barArea.getHeight() <= 0)
    return;

  int handle = handleAtPixel(e.position);
  bool changed = (handle != hoverHandle_);
  hoverHandle_ = handle;

  bool ghostWasVisible = ghostVisible_;
  auto prevGhostPos = ghostPos_;
  updateGhost(e.position);
  changed = changed || (ghostWasVisible != ghostVisible_);
  if (ghostVisible_)
    changed = changed || ghostPos_.getDistanceFrom(prevGhostPos) > 0.5f;

  // Cut handles only move horizontally; peak nodes move on both axes
  if (hoverHandle_ == kNoHandle)
    setMouseCursor(juce::MouseCursor::NormalCursor);
  else if (hoverHandle_ == kLowCutHandle || hoverHandle_ == kHighCutHandle)
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
  else
    setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);

  if (changed)
    repaint();
}

void GraphicEQDisplay::mouseDown(const juce::MouseEvent& e)
{
  auto barArea = getBarArea();
  if (barArea.getWidth() <= 0 || barArea.getHeight() <= 0)
    return;

  // Mouse interaction hides the focus ring; the cursor and the enlarged dot
  // already mark the target, and the next key press brings the ring back
  focusRingVisible_ = false;

  // Only the first click of a multi-click sequence resets the creation marker,
  // so mouseDoubleClick can tell a just-created handle from a pre-existing one
  if (e.getNumberOfClicks() == 1)
    justCreatedHandle_ = kNoHandle;

  dragHandle_ = handleAtPixel(e.position);
  ghostVisible_ = false;

  if (dragHandle_ == kNoHandle)
  {
    float normX = (e.position.x - static_cast<float>(barArea.getX())) /
                  static_cast<float>(barArea.getWidth());
    if (normX < 0.0f || normX > 1.0f)
    {
      DBG("Click outside the EQ bar area, ignoring");
      repaint();
      return;
    }

    float freqHz = normXToFreq(normX);

    if (isInLowCutZone(normX))
    {
      lowCut_.active = true;
      lowCut_.freqHz = freqHz;
      dragHandle_ = kLowCutHandle;
      justCreatedHandle_ = kLowCutHandle;
      dragScope_ = GestureScope::All;
      if (onDragStart)
        onDragStart(kLowCutHandle, dragScope_);
      if (onLowCutChanged)
        onLowCutChanged(true, freqHz);
      repaint();
    }
    else if (isInHighCutZone(normX))
    {
      highCut_.active = true;
      highCut_.freqHz = freqHz;
      dragHandle_ = kHighCutHandle;
      justCreatedHandle_ = kHighCutHandle;
      dragScope_ = GestureScope::All;
      if (onDragStart)
        onDragStart(kHighCutHandle, dragScope_);
      if (onHighCutChanged)
        onHighCutChanged(true, freqHz);
      repaint();
    }
    else
    {
      int slot = firstFreeSlot();
      if (slot < 0)
      {
        DBG("All " + juce::String(kNumNodes) + " EQ node slots in use, ignoring click");
        return;
      }

      float gainDb = yToGain(e.position.y, static_cast<float>(barArea.getY()),
                             static_cast<float>(barArea.getHeight()));

      auto& node = nodes_[static_cast<size_t>(slot)];
      node.active = true;
      node.freqHz = freqHz;
      node.gainDb = gainDb;
      dragHandle_ = slot;
      justCreatedHandle_ = slot;
      dragScope_ = GestureScope::All;

      if (onDragStart)
        onDragStart(slot, dragScope_);
      if (onNodeChanged)
        onNodeChanged(slot, true, freqHz, gainDb);
      repaint();
    }
  }
  else
  {
    // Dragging an existing handle never touches its 'active' parameter
    dragScope_ = GestureScope::Move;
    if (onDragStart)
      onDragStart(dragHandle_, dragScope_);
  }

  focusedHandle_ = dragHandle_;
  dragStartPos_ = e.position;
  if (dragHandle_ >= 0 && dragHandle_ < kNumNodes)
    dragStartGainDb_ = nodes_[static_cast<size_t>(dragHandle_)].gainDb;
}

void GraphicEQDisplay::mouseDrag(const juce::MouseEvent& e)
{
  if (dragHandle_ == kNoHandle)
    return;

  auto barArea = getBarArea();
  if (barArea.getWidth() <= 0 || barArea.getHeight() <= 0)
    return;

  float normX =
      (e.position.x - static_cast<float>(barArea.getX())) / static_cast<float>(barArea.getWidth());
  float newFreq = normXToFreq(normX);

  if (dragHandle_ == kLowCutHandle)
  {
    lowCut_.freqHz = newFreq;
    if (onLowCutChanged)
      onLowCutChanged(true, newFreq);
  }
  else if (dragHandle_ == kHighCutHandle)
  {
    highCut_.freqHz = newFreq;
    if (onHighCutChanged)
      onHighCutChanged(true, newFreq);
  }
  else
  {
    float areaH = static_cast<float>(barArea.getHeight());
    float deltaY = dragStartPos_.y - e.position.y;
    float deltaGain = (deltaY / areaH) * (kMaxGainDb - kMinGainDb);
    float newGain = juce::jlimit(kMinGainDb, kMaxGainDb, dragStartGainDb_ + deltaGain);

    auto& node = nodes_[static_cast<size_t>(dragHandle_)];
    node.freqHz = newFreq;
    node.gainDb = newGain;

    if (onNodeChanged)
      onNodeChanged(dragHandle_, true, newFreq, newGain);
  }
  repaint();
}

void GraphicEQDisplay::mouseUp(const juce::MouseEvent&)
{
  endDrag();
}

void GraphicEQDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
  int handle = handleAtPixel(e.position);
  if (handle == kNoHandle)
    return;

  if (handle == justCreatedHandle_)
  {
    // The first click of this double-click created the handle; deleting it
    // again would pollute host automation with a create-then-delete pair
    DBG("Double-click on a just-created handle, keeping it");
    justCreatedHandle_ = kNoHandle;
    return;
  }

  // Close the move-scope gesture the second click's mouseDown opened, then
  // bracket the deactivation in its own gesture that includes 'active'
  endDrag();
  if (onDragStart)
    onDragStart(handle, GestureScope::All);

  if (handle == kLowCutHandle)
  {
    lowCut_.active = false;
    if (onLowCutChanged)
      onLowCutChanged(false, lowCut_.freqHz);
  }
  else if (handle == kHighCutHandle)
  {
    highCut_.active = false;
    if (onHighCutChanged)
      onHighCutChanged(false, highCut_.freqHz);
  }
  else
  {
    auto& node = nodes_[static_cast<size_t>(handle)];
    node.active = false;
    // Report the stored gain so the parameter keeps it for re-activation
    if (onNodeChanged)
      onNodeChanged(handle, false, node.freqHz, node.gainDb);
  }

  if (onDragEnd)
    onDragEnd(handle, GestureScope::All);

  hoverHandle_ = kNoHandle;
  if (focusedHandle_ == handle)
    focusedHandle_ = kNoHandle;
  repaint();
}

void GraphicEQDisplay::mouseExit(const juce::MouseEvent&)
{
  if (hoverHandle_ != kNoHandle || ghostVisible_)
  {
    hoverHandle_ = kNoHandle;
    ghostVisible_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
  }
}

juce::Rectangle<int> GraphicEQDisplay::getBarArea() const
{
  auto content = getLocalBounds().reduced(LCDSpectrumDisplay::kPad);
  content.removeFromLeft(LCDSpectrumDisplay::kYAxisW);
  content.removeFromTop(LCDSpectrumDisplay::kTopPad);
  content.removeFromBottom(LCDSpectrumDisplay::kXAxisH);
  return content;
}

float GraphicEQDisplay::gainToY(float gainDb, float barAreaTop, float barAreaHeight) const
{
  float normalized = (gainDb - kMinGainDb) / (kMaxGainDb - kMinGainDb);
  return barAreaTop + barAreaHeight * (1.0f - normalized);
}

float GraphicEQDisplay::yToGain(float y, float barAreaTop, float barAreaHeight) const
{
  float normalized = 1.0f - (y - barAreaTop) / barAreaHeight;
  return juce::jlimit(kMinGainDb, kMaxGainDb, kMinGainDb + normalized * (kMaxGainDb - kMinGainDb));
}

bool GraphicEQDisplay::anyFilterActive() const
{
  if (lowCut_.active || highCut_.active)
    return true;

  for (const auto& node : nodes_)
  {
    if (node.active && std::fabs(node.gainDb) >= octob::GraphicEQBypassGainDb)
      return true;
  }
  return false;
}

float GraphicEQDisplay::responseAtFreq(float freqHz) const
{
  return octob::GraphicEQ::computeMagnitudeResponseDb(nodes_.data(), kNumNodes, lowCut_, highCut_,
                                                      freqHz, sampleRate_);
}

bool GraphicEQDisplay::handleActive(int handle) const
{
  if (handle == kLowCutHandle)
    return lowCut_.active;
  if (handle == kHighCutHandle)
    return highCut_.active;
  return getNodeActive(handle);
}

float GraphicEQDisplay::handleFrequency(int handle) const
{
  if (handle == kLowCutHandle)
    return lowCut_.freqHz;
  if (handle == kHighCutHandle)
    return highCut_.freqHz;
  return getNodeFrequency(handle);
}

// Pixel position of a handle's dot: x from its frequency, y from the combined
// response at that frequency so the dot rides the drawn curve
juce::Point<float> GraphicEQDisplay::handlePixel(int handle, juce::Rectangle<int> barArea) const
{
  float freqHz = handleFrequency(handle);
  float x = static_cast<float>(barArea.getX()) +
            freqToNormX(freqHz) * static_cast<float>(barArea.getWidth());
  float responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(freqHz));
  float y = gainToY(responseDb, static_cast<float>(barArea.getY()),
                    static_cast<float>(barArea.getHeight()));
  return {x, y};
}

int GraphicEQDisplay::handleAtPixel(juce::Point<float> p) const
{
  constexpr int kNumHandles = kNumNodes + 2;
  auto barArea = getBarArea();
  std::array<bool, kNumHandles> active{};
  std::array<float, kNumHandles> xs{};
  std::array<float, kNumHandles> ys{};
  for (int handle = 0; handle < kNumHandles; ++handle)
  {
    auto idx = static_cast<size_t>(handle);
    active[idx] = handleActive(handle);
    if (!active[idx])
      continue;
    auto pt = handlePixel(handle, barArea);
    xs[idx] = pt.x;
    ys[idx] = pt.y;
  }
  return nearestNodeIndex(active.data(), xs.data(), ys.data(), kNumHandles, p.x, p.y, kHitRadiusPx);
}

// Show a ghost dot on the response curve when the cursor is near the line,
// hinting that clicking will create a node there
void GraphicEQDisplay::updateGhost(juce::Point<float> p)
{
  auto barArea = getBarArea();
  float normX = (p.x - static_cast<float>(barArea.getX())) / static_cast<float>(barArea.getWidth());

  if (hoverHandle_ != kNoHandle || normX < 0.0f || normX > 1.0f)
  {
    ghostVisible_ = false;
    return;
  }

  float responseDb = juce::jlimit(kMinGainDb, kMaxGainDb, responseAtFreq(normXToFreq(normX)));
  float curveY = gainToY(responseDb, static_cast<float>(barArea.getY()),
                         static_cast<float>(barArea.getHeight()));

  ghostVisible_ = std::fabs(p.y - curveY) <= kGhostShowRadiusPx;
  ghostPos_ = {p.x, curveY};
}

int GraphicEQDisplay::firstFreeSlot() const
{
  for (int i = 0; i < kNumNodes; ++i)
  {
    if (!nodes_[static_cast<size_t>(i)].active)
      return i;
  }
  DBG("No free EQ node slot available");
  return -1;
}

void GraphicEQDisplay::endDrag()
{
  if (dragHandle_ == kNoHandle)
    return;

  if (onDragEnd)
    onDragEnd(dragHandle_, dragScope_);
  dragHandle_ = kNoHandle;
}

// --- Keyboard interaction ---

namespace
{
// One musical semitone per arrow press: fine enough for placement, coarse
// enough that traversing the full 20 Hz - 20 kHz range stays manageable
constexpr float kFreqNudgeRatio = 1.059463094f;  // 2^(1/12)
constexpr float kGainNudgeDb = 0.5f;
}  // namespace

bool GraphicEQDisplay::keyPressed(const juce::KeyPress& key)
{
  if (key.isKeyCode(juce::KeyPress::tabKey))
  {
    focusRingVisible_ = true;
    cycleFocusedHandle(key.getModifiers().isShiftDown() ? -1 : 1);
    repaint();
    return true;
  }

  if (!handleActive(focusedHandle_))
  {
    DBG("Key press with no focused EQ handle, ignoring");
    return false;
  }

  if (key.isKeyCode(juce::KeyPress::leftKey) || key.isKeyCode(juce::KeyPress::rightKey))
  {
    focusRingVisible_ = true;
    nudgeFocusedFrequency(key.isKeyCode(juce::KeyPress::rightKey) ? 1 : -1);
    return true;
  }

  if (key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey))
  {
    focusRingVisible_ = true;
    nudgeFocusedGain(key.isKeyCode(juce::KeyPress::upKey) ? 1 : -1);
    return true;
  }

  if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey))
  {
    focusRingVisible_ = true;
    removeFocusedHandle();
    return true;
  }

  return false;
}

void GraphicEQDisplay::focusGained(FocusChangeType cause)
{
  // Only keyboard-driven focus shows the ring immediately; a mouse click
  // reaches here too, but the pointer is its own indicator
  focusRingVisible_ = cause == FocusChangeType::focusChangedByTabKey;
  if (!handleActive(focusedHandle_))
    cycleFocusedHandle(1);
  repaint();
}

void GraphicEQDisplay::focusLost(FocusChangeType /*cause*/)
{
  repaint();
}

void GraphicEQDisplay::cycleFocusedHandle(int direction)
{
  constexpr int kNumHandles = kNumNodes + 2;
  int start = (focusedHandle_ == kNoHandle) ? (direction > 0 ? -1 : kNumHandles) : focusedHandle_;

  for (int step = 1; step <= kNumHandles; ++step)
  {
    int candidate = (start + direction * step + kNumHandles * step) % kNumHandles;
    if (handleActive(candidate))
    {
      focusedHandle_ = candidate;
      repaint();
      return;
    }
  }

  DBG("No active EQ handle to focus");
  focusedHandle_ = kNoHandle;
}

// Reports the focused handle's current values through the change callbacks,
// bracketed in a move-scope gesture so hosts record the nudge as automation
void GraphicEQDisplay::notifyHandleChanged(int handle)
{
  if (handle == kLowCutHandle)
  {
    if (onLowCutChanged)
      onLowCutChanged(lowCut_.active, lowCut_.freqHz);
  }
  else if (handle == kHighCutHandle)
  {
    if (onHighCutChanged)
      onHighCutChanged(highCut_.active, highCut_.freqHz);
  }
  else if (onNodeChanged)
  {
    const auto& node = nodes_[static_cast<size_t>(handle)];
    onNodeChanged(handle, node.active, node.freqHz, node.gainDb);
  }
}

void GraphicEQDisplay::nudgeFocusedFrequency(int direction)
{
  float ratio = direction > 0 ? kFreqNudgeRatio : 1.0f / kFreqNudgeRatio;
  float* freq = nullptr;
  if (focusedHandle_ == kLowCutHandle)
    freq = &lowCut_.freqHz;
  else if (focusedHandle_ == kHighCutHandle)
    freq = &highCut_.freqHz;
  else
    freq = &nodes_[static_cast<size_t>(focusedHandle_)].freqHz;

  *freq = juce::jlimit(kMinFreqHz, kMaxFreqHz, *freq * ratio);

  if (onDragStart)
    onDragStart(focusedHandle_, GestureScope::Move);
  notifyHandleChanged(focusedHandle_);
  if (onDragEnd)
    onDragEnd(focusedHandle_, GestureScope::Move);
  repaint();
}

void GraphicEQDisplay::nudgeFocusedGain(int direction)
{
  if (focusedHandle_ == kLowCutHandle || focusedHandle_ == kHighCutHandle)
  {
    DBG("Cut handles have no gain to nudge");
    return;
  }

  auto& node = nodes_[static_cast<size_t>(focusedHandle_)];
  node.gainDb = juce::jlimit(kMinGainDb, kMaxGainDb,
                             node.gainDb + kGainNudgeDb * static_cast<float>(direction));

  if (onDragStart)
    onDragStart(focusedHandle_, GestureScope::Move);
  notifyHandleChanged(focusedHandle_);
  if (onDragEnd)
    onDragEnd(focusedHandle_, GestureScope::Move);
  repaint();
}

void GraphicEQDisplay::removeFocusedHandle()
{
  int handle = focusedHandle_;
  if (onDragStart)
    onDragStart(handle, GestureScope::All);

  if (handle == kLowCutHandle)
  {
    lowCut_.active = false;
    if (onLowCutChanged)
      onLowCutChanged(false, lowCut_.freqHz);
  }
  else if (handle == kHighCutHandle)
  {
    highCut_.active = false;
    if (onHighCutChanged)
      onHighCutChanged(false, highCut_.freqHz);
  }
  else
  {
    auto& node = nodes_[static_cast<size_t>(handle)];
    node.active = false;
    if (onNodeChanged)
      onNodeChanged(handle, false, node.freqHz, node.gainDb);
  }

  if (onDragEnd)
    onDragEnd(handle, GestureScope::All);

  focusedHandle_ = kNoHandle;
  cycleFocusedHandle(1);
  repaint();
}
