#include "octobass-core/Compressor.hpp"

#include <algorithm>

namespace octob
{

Compressor::Compressor()
    : activeMode_(&vca_),
      sampleRate_(44100.0),
      squash_(DefaultSquashAmount),
      mode_(DefaultCompressionMode)
{
}

void Compressor::setSampleRate(SampleRate sampleRate)
{
  sampleRate_ = sampleRate;
  vca_.setSampleRate(sampleRate);
  opto_.setSampleRate(sampleRate);
  fet_.setSampleRate(sampleRate);
  bus_.setSampleRate(sampleRate);
}

void Compressor::setSquash(float amount)
{
  squash_ = std::clamp(amount, MinSquashAmount, MaxSquashAmount);
  activeMode_->setAmount(squash_);
}

void Compressor::setMode(int mode)
{
  int clamped = std::clamp(mode, 0, NumCompressionModes - 1);

  if (clamped == mode_)
    return;

  mode_ = clamped;

  switch (mode_)
  {
    case 0:
      activeMode_ = &vca_;
      break;
    case 1:
      activeMode_ = &opto_;
      break;
    case 2:
      activeMode_ = &fet_;
      break;
    case 3:
      activeMode_ = &bus_;
      break;
    default:
      activeMode_ = &vca_;
      break;
  }

  activeMode_->reset();
  activeMode_->setAmount(squash_);
}

void Compressor::process(const Sample* input, Sample* output, FrameCount numFrames)
{
  activeMode_->process(input, output, numFrames);
}

void Compressor::reset()
{
  vca_.reset();
  opto_.reset();
  fet_.reset();
  bus_.reset();
}

float Compressor::getGainReductionDb() const
{
  return activeMode_->getGainReductionDb();
}

float Compressor::getStaticMakeupDb() const
{
  return activeMode_->getStaticMakeupDb();
}

}  // namespace octob
