#include <gtest/gtest.h>
#include "TBeam1WFanControl.h"

TEST(TBeam1WFanControl, ConvertsManufacturerThermistorMidpoint) {
  EXPECT_NEAR(tbeam1w::ntcTemperatureC(1650), 25.0f, 0.01f);
  EXPECT_LT(tbeam1w::ntcTemperatureC(1000), 25.0f);
  EXPECT_GT(tbeam1w::ntcTemperatureC(2400), 45.0f);
}

TEST(TBeam1WFanControl, RejectsDisconnectedShortedAndSaturatedSensor) {
  for (uint32_t mv : {0u, 99u, 3001u, 3300u, 65535u}) {
    EXPECT_FALSE(std::isfinite(tbeam1w::ntcTemperatureC(mv)));
  }
  EXPECT_TRUE(std::isfinite(tbeam1w::ntcTemperatureC(100)));
  EXPECT_TRUE(std::isfinite(tbeam1w::ntcTemperatureC(3000)));
}

TEST(TBeam1WFanControl, StartsOnUntilBelowForty) {
  tbeam1w::FanControl fan;
  EXPECT_TRUE(fan.update(NAN));
  EXPECT_TRUE(fan.update(42.0f));
  EXPECT_TRUE(fan.update(40.0f));
  EXPECT_FALSE(fan.update(39.9f));
}

TEST(TBeam1WFanControl, UsesExactOnThresholdAndHysteresis) {
  tbeam1w::FanControl fan;
  EXPECT_FALSE(fan.update(25.0f));
  EXPECT_FALSE(fan.update(40.0f));
  EXPECT_FALSE(fan.update(44.9f));
  EXPECT_TRUE(fan.update(45.0f));
  EXPECT_TRUE(fan.update(70.0f));
  EXPECT_TRUE(fan.update(44.9f));
  EXPECT_TRUE(fan.update(40.0f));
  EXPECT_FALSE(fan.update(39.9f));
}

TEST(TBeam1WFanControl, SensorFaultForcesOnAndRecoveryRequiresCoolReading) {
  for (float invalid : {NAN, INFINITY, -INFINITY}) {
    tbeam1w::FanControl fan;
    ASSERT_FALSE(fan.update(25.0f));
    EXPECT_TRUE(fan.update(invalid));
    EXPECT_TRUE(fan.update(42.0f));
    EXPECT_FALSE(fan.update(25.0f));
  }
}
