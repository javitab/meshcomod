#include <gtest/gtest.h>
#include "TBeam1WRadio.h"

TEST(TBeam1WRadio, PreservesRampThroughStartupAndRuntimePowerChanges) {
  Module module;
  TBeam1WRadio radio(&module);
  CustomSX1262* driver = &radio;
  for (int8_t power : {22, 5, 0, -9, 22}) {
    EXPECT_EQ(driver->setOutputPower(power), RADIOLIB_ERR_NONE);
    EXPECT_EQ(radio.ramp, RADIOLIB_SX126X_PA_RAMP_1700U);
    EXPECT_TRUE(radio.optimized);
    EXPECT_EQ(radio.drive, 22);
  }
  EXPECT_EQ(radio.calls, (std::vector<char>{'P','R','P','R','P','R','P','R','P','R'}));
}

TEST(TBeam1WRadio, PreservesUnoptimizedPowerSelection) {
  Module module;
  TBeam1WRadio radio(&module);
  EXPECT_EQ(radio.setOutputPower(-5, false), RADIOLIB_ERR_NONE);
  EXPECT_FALSE(radio.optimized);
  EXPECT_EQ(radio.drive, -5);
  EXPECT_EQ(radio.ramp, RADIOLIB_SX126X_PA_RAMP_1700U);
}

TEST(TBeam1WRadio, BlocksTransmitUntilPowerIsConfigured) {
  Module module;
  TBeam1WRadio radio(&module);
  uint8_t data[] = {1};
  Serial.errors = 0;
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), RADIOLIB_ERR_UNKNOWN);
  EXPECT_TRUE(radio.calls.empty());
  EXPECT_EQ(Serial.errors, 1);
  ASSERT_EQ(radio.setOutputPower(22), RADIOLIB_ERR_NONE);
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), RADIOLIB_ERR_NONE);
  EXPECT_EQ(radio.calls, (std::vector<char>{'P','R','T'}));
}

TEST(TBeam1WRadio, PowerFailureIsReportedAndBlocksTransmit) {
  Module module;
  TBeam1WRadio radio(&module);
  ASSERT_EQ(radio.setOutputPower(22), RADIOLIB_ERR_NONE);
  radio.calls.clear();
  radio.power_result = -2;
  Serial.errors = 0;
  uint8_t data[] = {1};
  EXPECT_EQ(radio.setOutputPower(5), -2);
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), -2);
  EXPECT_EQ(radio.calls, (std::vector<char>{'P'}));
  EXPECT_EQ(Serial.errors, 2);
}

TEST(TBeam1WRadio, RampFailureBlocksTransmitUntilSuccessfulReconfiguration) {
  Module module;
  TBeam1WRadio radio(&module);
  uint8_t data[] = {1};
  radio.ramp_result = -3;
  Serial.errors = 0;
  EXPECT_EQ(radio.setOutputPower(22), -3);
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), -3);
  EXPECT_EQ(radio.calls, (std::vector<char>{'P','R'}));
  EXPECT_EQ(Serial.errors, 2);
  radio.ramp_result = 0;
  ASSERT_EQ(radio.setOutputPower(5), RADIOLIB_ERR_NONE);
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), RADIOLIB_ERR_NONE);
  EXPECT_EQ(radio.calls.back(), 'T');
}

TEST(TBeam1WRadio, PropagatesTransmitErrors) {
  Module module;
  TBeam1WRadio radio(&module);
  uint8_t data[] = {1};
  ASSERT_EQ(radio.setOutputPower(22), RADIOLIB_ERR_NONE);
  radio.tx_result = -4;
  EXPECT_EQ(radio.startTransmit(data, sizeof(data)), -4);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
