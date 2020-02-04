// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_property_util.h"

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chromeos/constants/chromeos_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kCrosConfigPropertiesPath[] = "/arc/build-properties";

class FakeCrosConfig : public arc::CrosConfig {
 public:
  FakeCrosConfig() = default;
  ~FakeCrosConfig() override = default;
  FakeCrosConfig(const FakeCrosConfig&) = delete;
  FakeCrosConfig& operator=(const FakeCrosConfig&) = delete;

  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override {
    auto it = overrides_.find(property);
    if (it != overrides_.end()) {
      *val_out = it->second;
      return true;
    }
    return arc::CrosConfig::GetString(path, property, val_out);
  }

  void SetString(const std::string& path,
                 const std::string& property,
                 const std::string& value) {
    overrides_.emplace(property, value);
  }

 private:
  std::map<std::string, std::string> overrides_;
};

class ArcPropertyUtilTest : public testing::Test {
 public:
  ArcPropertyUtilTest() = default;
  ~ArcPropertyUtilTest() override = default;
  ArcPropertyUtilTest(const ArcPropertyUtilTest&) = delete;
  ArcPropertyUtilTest& operator=(const ArcPropertyUtilTest&) = delete;

  void SetUp() override { ASSERT_TRUE(dir_.CreateUniqueTempDir()); }

 protected:
  const base::FilePath& GetTempDir() const { return dir_.GetPath(); }

  CrosConfig* config() {
    if (!config_)
      config_ = std::make_unique<CrosConfig>();
    return config_.get();
  }

 private:
  std::unique_ptr<CrosConfig> config_;
  base::ScopedTempDir dir_;
};

TEST_F(ArcPropertyUtilTest, TestPropertyExpansions) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");

  std::string expanded;
  EXPECT_TRUE(ExpandPropertyContentsForTesting(
      "line1\n{brand}\nline3\n{brand} {brand}", &config, &expanded));
  EXPECT_EQ("line1\nalphabet\nline3\nalphabet alphabet\n", expanded);
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsUnmatchedBrace) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");

  std::string expanded;
  EXPECT_FALSE(ExpandPropertyContentsForTesting("line{1\nline}2\nline3",
                                                &config, &expanded));
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsRecursive) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;
  EXPECT_TRUE(ExpandPropertyContentsForTesting("{model}", &config, &expanded));
  EXPECT_EQ("alphabet soup\n", expanded);
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsMissingProperty) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;

  EXPECT_FALSE(ExpandPropertyContentsForTesting("{missing-property}", &config,
                                                &expanded));
  EXPECT_FALSE(ExpandPropertyContentsForTesting("{model}", &config, &expanded));
}

// Verify that ro.product.board gets copied to ro.oem.key1 as well.
TEST_F(ArcPropertyUtilTest, TestPropertyExpansionBoard) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "board", "testboard");

  std::string expanded;
  EXPECT_TRUE(ExpandPropertyContentsForTesting("ro.product.board={board}",
                                               &config, &expanded));
  EXPECT_EQ("ro.product.board=testboard\nro.oem.key1=testboard\n", expanded);
}

// Non-fingerprint property should do simple truncation.
TEST_F(ArcPropertyUtilTest, TestPropertyTruncation) {
  std::string truncated;
  EXPECT_TRUE(TruncateAndroidPropertyForTesting(
      "property.name="
      "012345678901234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901",
      &truncated));
  EXPECT_EQ(
      "property.name=0123456789012345678901234567890123456789"
      "012345678901234567890123456789012345678901234567890",
      truncated);
}

// Fingerprint truncation with /release-keys should do simple truncation.
TEST_F(ArcPropertyUtilTest, TestPropertyTruncationFingerprintRelease) {
  std::string truncated;
  EXPECT_TRUE(TruncateAndroidPropertyForTesting(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/"
      "release-keys",
      &truncated));
  EXPECT_EQ(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/relea",
      truncated);
}

// Fingerprint truncation with /dev-keys needs to preserve the /dev-keys.
TEST_F(ArcPropertyUtilTest, TestPropertyTruncationFingerprintDev) {
  std::string truncated;
  EXPECT_TRUE(TruncateAndroidPropertyForTesting(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1/R65-10299.0.9999/4538390:user/dev-keys",
      &truncated));
  EXPECT_EQ(
      "ro.bootimage.build.fingerprint=google/toolongdevicena/"
      "toolongdevicena_cheets/R65-10299.0.9999/4538390:user/dev-keys",
      truncated);
}

// Fingerprint truncation with the wrong format should fail.
TEST_F(ArcPropertyUtilTest, TestPropertyTruncationBadFingerprint) {
  std::string truncated;
  EXPECT_FALSE(TruncateAndroidPropertyForTesting(
      "ro.bootimage.build.fingerprint=google/toolongdevicename/"
      "toolongdevicename_cheets:7.1.1:123456789012345678901234567890/dev-keys",
      &truncated));
}

// Fingerprint truncation without enough room should fail.
TEST_F(ArcPropertyUtilTest, TestPropertyTruncationFingerprintShortDevice) {
  std::string truncated;
  EXPECT_FALSE(TruncateAndroidPropertyForTesting(
      "ro.bootimage.build.fingerprint=google/dev/"
      "dev_cheets:7.1.1/R65-10299.0.9999/453839012345678901234567890"
      "12345678901234567890:user/dev-keys",
      &truncated));
}

// Tests that the GetString method works as intended.
TEST_F(ArcPropertyUtilTest, CrosConfig_GetString) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"v1\",\"k2\":\"v2\",\"k3\":3}");
  std::string str;
  EXPECT_TRUE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
  EXPECT_EQ("v1", str);
  EXPECT_TRUE(config()->GetString(kCrosConfigPropertiesPath, "k2", &str));
  EXPECT_EQ("v2", str);
  // The value is not a string.
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k3", &str));
  // The property path is invalid.
  EXPECT_FALSE(config()->GetString("/unknown/path", "k1", &str));
}

// Tests that CrosConfig can handle the case where the command line is not
// passed.
TEST_F(ArcPropertyUtilTest, CrosConfig_NoCommandline) {
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle an empty command line.
TEST_F(ArcPropertyUtilTest, CrosConfig_EmptyCommandline) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties, "");
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle JSON whose top-level is not a dict.
TEST_F(ArcPropertyUtilTest, CrosConfig_NoDict) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "[\"k1\"]");
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that CrosConfig can handle an invalid JSON.
TEST_F(ArcPropertyUtilTest, CrosConfig_InvalidJson) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":}");  // parse error: no value
  std::string str;
  EXPECT_FALSE(config()->GetString(kCrosConfigPropertiesPath, "k1", &str));
}

// Tests that ExpandPropertyFile works as intended when no property expantion
// is needed.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_NoExpansion) {
  constexpr const char kValidProp[] = "ro.foo=bar\nro.baz=boo";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ(std::string(kValidProp) + "\n", content);
}

// Tests that ExpandPropertyFile works as intended when property expantion
// is needed.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_Expansion) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"v1\",\"k2\":\"v2\"}");
  constexpr const char kValidProp[] = "ro.foo={k1}\nro.baz={k2}";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ("ro.foo=v1\nro.baz=v2\n", content);
}

// Tests that ExpandPropertyFile works as intended when nested property
// expantion is needed.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_NestedExpansion) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(chromeos::switches::kArcBuildProperties,
                                  "{\"k1\":\"{k2}\",\"k2\":\"v2\"}");
  constexpr const char kValidProp[] = "ro.foo={k1}\nro.baz={k2}";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));

  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_TRUE(ExpandPropertyFile(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
  // Note: ExpandPropertyFile() adds a trailing LF.
  EXPECT_EQ("ro.foo=v2\nro.baz=v2\n", content);
}

// Test that ExpandPropertyFile handles the case where a property is not found.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_CannotExpand) {
  constexpr const char kValidProp[] =
      "ro.foo={nonexistent-property}\nro.baz=boo\n";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));
  const base::FilePath dest = GetTempDir().Append("new.prop");
  EXPECT_FALSE(ExpandPropertyFile(path, dest, config()));
}

// Test that ExpandPropertyFile handles the case where the input file is not
// found.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_NoSourceFile) {
  EXPECT_FALSE(ExpandPropertyFile(base::FilePath("/nonexistent"),
                                  base::FilePath("/nonexistent2"), config()));
}

// Test that ExpandPropertyFile handles the case where the output file cannot
// be written.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_CannotWrite) {
  constexpr const char kValidProp[] = "ro.foo=bar\nro.baz=boo\n";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));
  EXPECT_FALSE(
      ExpandPropertyFile(path, base::FilePath("/nonexistent2"), config()));
}

TEST_F(ArcPropertyUtilTest, ExpandPropertyFiles_NoSource) {
  // Both source and dest are not found.
  EXPECT_FALSE(ExpandPropertyFiles(base::FilePath("/nonexistent1"),
                                   base::FilePath("/nonexistent2")));

  // Both source and dest exist, but the source directory is empty.
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_dir));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir));

  // Add default.prop to the source, but not build.prop.
  base::FilePath default_prop = source_dir.Append("default.prop");
  constexpr const char kDefaultProp[] = "ro.foo=bar\n";
  base::WriteFile(default_prop, kDefaultProp, strlen(kDefaultProp));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir));

  // Add build.prop too. Then the call should succeed.
  base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] = "ro.baz=boo\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir));

  // Verify two dest files are there.
  EXPECT_TRUE(base::PathExists(dest_dir.Append("default.prop")));
  EXPECT_TRUE(base::PathExists(dest_dir.Append("build.prop")));

  // Verify their content.
  // Note: ExpandPropertyFile() adds a trailing LF.
  std::string content;
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);
  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  EXPECT_EQ(std::string(kBuildProp) + "\n", content);

  // Finally, test the case where source is valid but the dest is not.
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, base::FilePath("/nonexistent")));
}

}  // namespace
}  // namespace arc
