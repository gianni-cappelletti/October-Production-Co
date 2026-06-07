#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

/// ToggleButton that shows a value bubble while hovered, mirroring
/// juce::Slider's popup display (immediate on mouseEnter, refreshed on state
/// changes, dismissed on mouseExit) instead of the delayed TooltipWindow.
class PopupToggleButton : public juce::ToggleButton
{
 public:
  // Returns the text to display; called on hover and again on state changes
  std::function<juce::String()> popupTextProvider;

  void mouseEnter(const juce::MouseEvent& e) override
  {
    juce::ToggleButton::mouseEnter(e);
    showPopup();
  }

  void mouseExit(const juce::MouseEvent& e) override
  {
    juce::ToggleButton::mouseExit(e);
    popup_.reset();
  }

 protected:
  void buttonStateChanged() override
  {
    juce::ToggleButton::buttonStateChanged();
    if (popup_ != nullptr)
      showPopup();
  }

 private:
  class PopupDisplay : public juce::BubbleComponent
  {
   public:
    explicit PopupDisplay(PopupToggleButton& owner) : owner_(owner)
    {
      setTransform(juce::AffineTransform::scale(
          juce::Component::getApproximateScaleFactorForComponent(&owner)));
      setAlwaysOnTop(true);
      setLookAndFeel(&owner.getLookAndFeel());
    }

    ~PopupDisplay() override { setLookAndFeel(nullptr); }

    void show(const juce::String& text)
    {
      text_ = text;
      BubbleComponent::setPosition(&owner_);
      repaint();
    }

    void paintContent(juce::Graphics& g, int w, int h) override
    {
      g.setFont(font_);
      g.setColour(owner_.findColour(juce::TooltipWindow::textColourId, true));
      g.drawFittedText(text_, juce::Rectangle<int>(w, h), juce::Justification::centred, 1);
    }

    void getContentSize(int& w, int& h) override
    {
      w = juce::GlyphArrangement::getStringWidthInt(font_, text_) + 18;
      h = static_cast<int>(font_.getHeight() * 1.6f);
    }

   private:
    PopupToggleButton& owner_;
    // Same metrics as LookAndFeel_V4::getSliderPopupFont so the bubble
    // matches the trim sliders' popup displays
    juce::Font font_{juce::FontOptions(15.0f, juce::Font::bold)};
    juce::String text_;
  };

  void showPopup()
  {
    if (popupTextProvider == nullptr)
      return;

    if (popup_ == nullptr)
    {
      popup_ = std::make_unique<PopupDisplay>(*this);
      popup_->addToDesktop(juce::ComponentPeer::windowIsTemporary |
                           juce::ComponentPeer::windowIgnoresKeyPresses |
                           juce::ComponentPeer::windowIgnoresMouseClicks);
      popup_->setVisible(true);
    }
    popup_->show(popupTextProvider());
  }

  std::unique_ptr<PopupDisplay> popup_;
};
