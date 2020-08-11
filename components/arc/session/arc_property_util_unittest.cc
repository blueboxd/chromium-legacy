// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_property_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/test/fake_cros_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kCrosConfigPropertiesPath[] = "/arc/build-properties";

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
      "ro.a=line1\nro.b={brand}\nro.c=line3\nro.d={brand} {brand}", &config,
      &expanded));
  EXPECT_EQ("ro.a=line1\nro.b=alphabet\nro.c=line3\nro.d=alphabet alphabet\n",
            expanded);
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsUnmatchedBrace) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");

  std::string expanded;
  EXPECT_FALSE(ExpandPropertyContentsForTesting(
      "ro.a=line{1\nro.b=line}2\nro.c=line3", &config, &expanded));
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsRecursive) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "brand", "alphabet");
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;
  EXPECT_TRUE(
      ExpandPropertyContentsForTesting("ro.a={model}", &config, &expanded));
  EXPECT_EQ("ro.a=alphabet soup\n", expanded);
}

TEST_F(ArcPropertyUtilTest, TestPropertyExpansionsMissingProperty) {
  FakeCrosConfig config;
  config.SetString("/arc/build-properties", "model", "{brand} soup");

  std::string expanded;

  EXPECT_FALSE(ExpandPropertyContentsForTesting("ro.a={missing-property}",
                                                &config, &expanded));
  EXPECT_FALSE(
      ExpandPropertyContentsForTesting("ro.a={model}", &config, &expanded));
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
  EXPECT_TRUE(ExpandPropertyFileForTesting(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
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
  EXPECT_TRUE(ExpandPropertyFileForTesting(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
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
  EXPECT_TRUE(ExpandPropertyFileForTesting(path, dest, config()));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest, &content));
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
  EXPECT_FALSE(ExpandPropertyFileForTesting(path, dest, config()));
}

// Test that ExpandPropertyFile handles the case where the input file is not
// found.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_NoSourceFile) {
  EXPECT_FALSE(ExpandPropertyFileForTesting(base::FilePath("/nonexistent"),
                                            base::FilePath("/nonexistent2"),
                                            config()));
}

// Test that ExpandPropertyFile handles the case where the output file cannot
// be written.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFile_CannotWrite) {
  constexpr const char kValidProp[] = "ro.foo=bar\nro.baz=boo\n";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidProp, strlen(kValidProp));
  EXPECT_FALSE(ExpandPropertyFileForTesting(
      path, base::FilePath("/nonexistent2"), config()));
}

TEST_F(ArcPropertyUtilTest, ExpandPropertyFiles) {
  // Both source and dest are not found.
  EXPECT_FALSE(ExpandPropertyFiles(base::FilePath("/nonexistent1"),
                                   base::FilePath("/nonexistent2"),
                                   /*single_file=*/false,
                                   /*add_native_bridge...=*/false));

  // Both source and dest exist, but the source directory is empty.
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_dir));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir, false, false));

  // Add default.prop to the source, but not build.prop.
  base::FilePath default_prop = source_dir.Append("default.prop");
  // Add a non-ro property to make sure that the property is NOT filetered out
  // when not in the "append" mode.
  constexpr const char kDefaultProp[] = "dalvik.a=b\nro.foo=bar\n";
  base::WriteFile(default_prop, kDefaultProp, strlen(kDefaultProp));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir, false, false));

  // Add build.prop too. The call should not succeed still.
  base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] = "ro.baz=boo\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir, false, false));

  // Add vendor_build.prop too. Then the call should succeed.
  base::FilePath vendor_build_prop = source_dir.Append("vendor_build.prop");
  constexpr const char kVendorBuildProp[] = "ro.a=b\n";
  base::WriteFile(vendor_build_prop, kVendorBuildProp,
                  strlen(kVendorBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir, false, false));

  // Verify all dest files are there.
  EXPECT_TRUE(base::PathExists(dest_dir.Append("default.prop")));
  EXPECT_TRUE(base::PathExists(dest_dir.Append("build.prop")));
  EXPECT_TRUE(base::PathExists(dest_dir.Append("vendor_build.prop")));

  // Verify their content.
  std::string content;
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);
  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  EXPECT_EQ(std::string(kBuildProp) + "\n", content);
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("vendor_build.prop"), &content));
  EXPECT_EQ(std::string(kVendorBuildProp) + "\n", content);

  // Expand it again, verify the previous result is cleared.
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir, false, false));
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);

  // If default.prop does not exist in the source path, it should still process
  // the other files, while also ensuring that default.prop is removed from the
  // destination path.
  base::DeleteFile(dest_dir.Append("default.prop"));

  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir, false, false));

  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  EXPECT_EQ(std::string(kBuildProp) + "\n", content);
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("vendor_build.prop"), &content));
  EXPECT_EQ(std::string(kVendorBuildProp) + "\n", content);

  // Finally, test the case where source is valid but the dest is not.
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, base::FilePath("/nonexistent"),
                                   false, false));
}

// Do the same as the previous test, but with |single_file| == true.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFiles_SingleFile) {
  // Both source and dest are not found.
  EXPECT_FALSE(ExpandPropertyFiles(base::FilePath("/nonexistent1"),
                                   base::FilePath("/nonexistent2"),
                                   /*single_file=*/true,
                                   /*add_native_bridge...=*/false));

  // Both source and dest exist, but the source directory is empty.
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_prop_file;
  ASSERT_TRUE(
      base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_prop_file));
  dest_prop_file = dest_prop_file.Append("combined.prop");
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Add default.prop to the source, but not build.prop.
  const base::FilePath default_prop = source_dir.Append("default.prop");
  // Add a non-ro property to make sure that the property is filetered out when
  // in the "append" mode.
  constexpr const char kDefaultPropNonRo[] = "dalvik.a=b\n";
  constexpr const char kDefaultProp[] = "ro.foo=bar\n";
  base::WriteFile(default_prop,
                  base::StringPrintf("%s%s", kDefaultPropNonRo, kDefaultProp));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Add build.prop too. The call should not succeed still.
  const base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] = "ro.baz=boo\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Add vendor_build.prop too. Then the call should succeed.
  const base::FilePath vendor_build_prop =
      source_dir.Append("vendor_build.prop");
  constexpr const char kVendorBuildProp[] = "ro.a=b\n";
  base::WriteFile(vendor_build_prop, kVendorBuildProp,
                  strlen(kVendorBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Add other optional files too. Then the call should succeed.
  const base::FilePath system_ext_build_prop =
      source_dir.Append("system_ext_build.prop");
  constexpr const char kSystemExtBuildProp[] = "ro.c=d\n";
  base::WriteFile(system_ext_build_prop, kSystemExtBuildProp,
                  strlen(kSystemExtBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  const base::FilePath odm_build_prop = source_dir.Append("odm_build.prop");
  constexpr const char kOdmBuildProp[] = "ro.e=f\n";
  base::WriteFile(odm_build_prop, kOdmBuildProp, strlen(kOdmBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  const base::FilePath product_build_prop =
      source_dir.Append("product_build.prop");
  constexpr const char kProductBuildProp[] = "ro.g=h\n";
  base::WriteFile(product_build_prop, kProductBuildProp,
                  strlen(kProductBuildProp));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Verify only one dest file exists.
  EXPECT_FALSE(
      base::PathExists(dest_prop_file.DirName().Append("default.prop")));
  EXPECT_FALSE(base::PathExists(dest_prop_file.DirName().Append("build.prop")));
  EXPECT_FALSE(
      base::PathExists(dest_prop_file.DirName().Append("vendor_build.prop")));
  EXPECT_FALSE(base::PathExists(
      dest_prop_file.DirName().Append("system_ext_build.prop")));
  EXPECT_FALSE(
      base::PathExists(dest_prop_file.DirName().Append("odm_build.prop")));
  EXPECT_FALSE(
      base::PathExists(dest_prop_file.DirName().Append("product_build.prop")));
  EXPECT_TRUE(base::PathExists(dest_prop_file));

  // Verify the content.
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest_prop_file, &content));
  // Don't include kDefaultPropNonRo since that one should be filtered out.
  EXPECT_EQ(base::StringPrintf("%s%s%s%s%s%s", kDefaultProp, kBuildProp,
                               kSystemExtBuildProp, kVendorBuildProp,
                               kOdmBuildProp, kProductBuildProp),
            content);

  // Expand it again, verify the previous result is cleared.
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));
  EXPECT_TRUE(base::ReadFileToString(dest_prop_file, &content));
  EXPECT_EQ(base::StringPrintf("%s%s%s%s%s%s", kDefaultProp, kBuildProp,
                               kSystemExtBuildProp, kVendorBuildProp,
                               kOdmBuildProp, kProductBuildProp),
            content);

  // If optional ones e.g. default.prop does not exist in the source path, it
  // should still process the other files.
  base::DeleteFile(source_dir.Append("default.prop"));
  base::DeleteFile(source_dir.Append("odm_build.prop"));
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));
  EXPECT_TRUE(base::ReadFileToString(dest_prop_file, &content));
  EXPECT_EQ(base::StringPrintf("%s%s%s%s", kBuildProp, kSystemExtBuildProp,
                               kVendorBuildProp, kProductBuildProp),
            content);

  // Finally, test the case where source is valid but the dest is not.
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, base::FilePath("/nonexistent"),
                                   true, false));
}

// Test that ExpandPropertyFiles handles properties related to native bridge
// 64-bit support properly.
TEST_F(ArcPropertyUtilTest, TestNativeBridge64Support) {
  // Set up some properties files.
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_dir));

  base::FilePath default_prop = source_dir.Append("default.prop");
  constexpr const char kDefaultProp[] = "ro.foo=bar\n";
  base::WriteFile(default_prop, kDefaultProp, strlen(kDefaultProp));

  base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] =
      "ro.baz=boo\n"
      "ro.product.cpu.abilist=x86_64,x86,armeabi-v7a,armeabi\n"
      "ro.product.cpu.abilist64=x86_64\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));

  base::FilePath vendor_build_prop = source_dir.Append("vendor_build.prop");
  constexpr const char kVendorBuildProp[] =
      "ro.a=b\n"
      "ro.vendor.product.cpu.abilist=x86_64,x86,armeabi-v7a,armeabi\n"
      "ro.vendor.product.cpu.abilist64=x86_64\n";
  base::WriteFile(vendor_build_prop, kVendorBuildProp,
                  strlen(kVendorBuildProp));

  // Expand with experiment off, verify properties are untouched.
  std::string content;
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir, false, false));
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);
  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  EXPECT_EQ(std::string(kBuildProp) + "\n", content);
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("vendor_build.prop"), &content));
  EXPECT_EQ(std::string(kVendorBuildProp) + "\n", content);

  // Expand with experiment on, verify properties are added / modified in
  // build.prop but not other files.
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_dir, false, true));
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);
  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  constexpr const char kBuildPropModifiedFirst[] =
      "ro.baz=boo\n"
      "ro.product.cpu.abilist=x86_64,x86,arm64-v8a,armeabi-v7a,armeabi\n"
      "ro.product.cpu.abilist64=x86_64,arm64-v8a\n";
  constexpr const char kBuildPropModifiedSecond[] =
      "ro.dalvik.vm.isa.arm64=x86_64\n";
  EXPECT_EQ(base::StringPrintf("%s\n%s", kBuildPropModifiedFirst,
                               kBuildPropModifiedSecond),
            content);
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("vendor_build.prop"), &content));
  constexpr const char kVendorBuildPropModified[] =
      "ro.a=b\n"
      "ro.vendor.product.cpu.abilist=x86_64,x86,arm64-v8a,armeabi-v7a,armeabi\n"
      "ro.vendor.product.cpu.abilist64=x86_64,arm64-v8a\n";
  EXPECT_EQ(std::string(kVendorBuildPropModified) + "\n", content);

  // Expand to a single file with experiment on, verify properties are added /
  // modified as expected.
  base::FilePath dest_prop_file;
  ASSERT_TRUE(
      base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_prop_file));
  dest_prop_file = dest_prop_file.Append("combined.prop");
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, true));

  // Verify the contents.
  EXPECT_TRUE(base::ReadFileToString(dest_prop_file, &content));
  EXPECT_EQ(
      base::StringPrintf("%s%s%s%s", kDefaultProp, kBuildPropModifiedFirst,
                         kBuildPropModifiedSecond, kVendorBuildPropModified),
      content);

  // Verify that unexpected property values generate an error.
  constexpr const char kBuildPropUnexpected[] =
      "ro.baz=boo\n"
      "ro.product.cpu.abilist=x86_64,armeabi-v7a,armeabi,unexpected-abi\n"
      "ro.product.cpu.abilist64=x86_64\n";
  base::WriteFile(build_prop, kBuildPropUnexpected,
                  strlen(kBuildPropUnexpected));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir, false, true));
  constexpr const char kBuildPropUnexpected2[] =
      "ro.baz=boo\n"
      "ro.product.cpu.abilist=x86_64,x86,armeabi-v7a,armeabi\n"
      "ro.product.cpu.abilist64=x86_64,unexpected-abi_64\n";
  base::WriteFile(build_prop, kBuildPropUnexpected2,
                  strlen(kBuildPropUnexpected2));
  EXPECT_FALSE(ExpandPropertyFiles(source_dir, dest_dir, false, true));
}

// Verify that comments and non ro. properties are not written.
TEST_F(ArcPropertyUtilTest, ExpandPropertyFiles_SingleFile_NonRo) {
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_dir));

  const base::FilePath default_prop = source_dir.Append("default.prop");
  constexpr const char kDefaultProp[] = "###\ndalvik.foo=bar\nro.foo=bar\n";
  base::WriteFile(default_prop, kDefaultProp, strlen(kDefaultProp));

  const base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] = "###\ndalvik.baz=boo\nro.baz=boo\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));

  const base::FilePath vendor_build_prop =
      source_dir.Append("vendor_build.prop");
  constexpr const char kVendorBuildProp[] = "###\ndalvik.a=b\nro.a=b\n";
  base::WriteFile(vendor_build_prop, kVendorBuildProp,
                  strlen(kVendorBuildProp));

  const base::FilePath dest_prop_file = dest_dir.Append("combined.prop");
  EXPECT_TRUE(ExpandPropertyFiles(source_dir, dest_prop_file, true, false));

  // Verify the content.
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(dest_prop_file, &content));
  EXPECT_EQ("ro.foo=bar\nro.baz=boo\nro.a=b\n", content);
}

}  // namespace
}  // namespace arc
