#include <gtest/gtest.h>
#include <juce_events/juce_events.h>

class JUCETestEnvironment : public ::testing::Environment
{
 public:
  void SetUp() override { juce::initialiseJuce_GUI(); }
  void TearDown() override { juce::shutdownJuce_GUI(); }
};

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new JUCETestEnvironment());
  return RUN_ALL_TESTS();
}
