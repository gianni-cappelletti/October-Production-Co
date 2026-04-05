#include <gtest/gtest.h>
#include <juce_gui_basics/juce_gui_basics.h>

int main(int argc, char** argv)
{
  juce::ScopedJuceInitialiser_GUI juceInit;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
