#include <gtest/gtest.h>
#include <nvs.h>
#include "RadioModeStore.h"

class RadioModeStoreTest : public ::testing::Test {
protected:
  void SetUp() override {
    mock_nvs.clear();
    mock_namespace_exists = false;
    mock_open_failure = mock_read_failure = mock_write_failure = mock_commit_failure = false;
  }
};

TEST_F(RadioModeStoreTest, FreshDeviceDefaultsToCompanionWithoutWriting) {
  CompanionRadioMode mode = CompanionRadioMode::KissTcp;
  const char* error;
  ASSERT_TRUE(radioModeLoad(mode, error));
  EXPECT_EQ(mode, CompanionRadioMode::Companion);
  EXPECT_EQ(error, nullptr);
  EXPECT_FALSE(mock_namespace_exists);
}

TEST_F(RadioModeStoreTest, ModeSurvivesReloadAndCanReturnToCompanion) {
  const char* error;
  CompanionRadioMode mode;
  ASSERT_TRUE(radioModeSave(CompanionRadioMode::KissTcp, error));
  ASSERT_TRUE(radioModeLoad(mode, error));
  EXPECT_EQ(mode, CompanionRadioMode::KissTcp);
  ASSERT_TRUE(radioModeSave(CompanionRadioMode::Companion, error));
  ASSERT_TRUE(radioModeLoad(mode, error));
  EXPECT_EQ(mode, CompanionRadioMode::Companion);
  EXPECT_EQ(mock_nvs.size(), 1U);
}

TEST_F(RadioModeStoreTest, MissingKeyDefaultsToCompanion) {
  mock_namespace_exists = true;
  const char* error;
  CompanionRadioMode mode;
  ASSERT_TRUE(radioModeLoad(mode, error));
  EXPECT_EQ(mode, CompanionRadioMode::Companion);
}

TEST_F(RadioModeStoreTest, CorruptModeDoesNotSilentlySelectCompanion) {
  mock_namespace_exists = true;
  mock_nvs["mode"] = 99;
  const char* error;
  CompanionRadioMode mode = CompanionRadioMode::KissTcp;
  EXPECT_FALSE(radioModeLoad(mode, error));
  EXPECT_NE(error, nullptr);
  EXPECT_EQ(mode, CompanionRadioMode::KissTcp);
}

TEST_F(RadioModeStoreTest, ReadErrorsAreExplicit) {
  const char* error;
  CompanionRadioMode mode;
  mock_open_failure = true;
  EXPECT_FALSE(radioModeLoad(mode, error));
  EXPECT_NE(error, nullptr);
  mock_open_failure = false;
  mock_namespace_exists = true;
  mock_read_failure = true;
  EXPECT_FALSE(radioModeLoad(mode, error));
  EXPECT_NE(error, nullptr);
}

TEST_F(RadioModeStoreTest, WriteAndCommitErrorsDoNotReportSuccess) {
  const char* error;
  mock_open_failure = true;
  EXPECT_FALSE(radioModeSave(CompanionRadioMode::KissTcp, error));
  EXPECT_NE(error, nullptr);
  mock_open_failure = false;
  mock_write_failure = true;
  EXPECT_FALSE(radioModeSave(CompanionRadioMode::KissTcp, error));
  EXPECT_NE(error, nullptr);
  mock_write_failure = false;
  mock_commit_failure = true;
  EXPECT_FALSE(radioModeSave(CompanionRadioMode::KissTcp, error));
  EXPECT_NE(error, nullptr);
  EXPECT_TRUE(mock_nvs.empty());
}

TEST_F(RadioModeStoreTest, InvalidModeIsNotWritten) {
  const char* error;
  EXPECT_FALSE(radioModeSave(static_cast<CompanionRadioMode>(42), error));
  EXPECT_NE(error, nullptr);
  EXPECT_FALSE(mock_namespace_exists);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
