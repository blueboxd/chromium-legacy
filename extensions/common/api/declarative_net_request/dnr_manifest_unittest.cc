// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace keys = manifest_keys;
namespace errors = manifest_errors;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {
namespace {

std::string GetRuleResourcesKey() {
  return base::JoinString(
      {keys::kDeclarativeNetRequestKey, keys::kDeclarativeRuleResourcesKey},
      ".");
}

// Fixture testing the kDeclarativeNetRequestKey manifest key.
class DNRManifestTest : public testing::Test {
 public:
  DNRManifestTest() : channel_(::version_info::Channel::UNKNOWN) {}

 protected:
  // Loads the extension and verifies the |expected_error|.
  void LoadAndExpectError(const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), Manifest::UNPACKED, Extension::NO_FLAGS, &error);
    EXPECT_FALSE(extension);
    EXPECT_EQ(expected_error, error);
  }

  // Loads the extension and verifies that the manifest info is correctly set
  // up.
  void LoadAndExpectSuccess(const std::vector<TestRulesetInfo>& info) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        temp_dir_.GetPath(), Manifest::UNPACKED, Extension::NO_FLAGS, &error);
    ASSERT_TRUE(extension) << error;
    EXPECT_TRUE(error.empty());

    const std::vector<DNRManifestData::RulesetInfo>& rulesets =
        DNRManifestData::GetRulesets(*extension);
    ASSERT_EQ(info.size(), rulesets.size());
    for (size_t i = 0; i < rulesets.size(); ++i) {
      EXPECT_EQ(RulesetID(i + kMinValidStaticRulesetID.value()),
                rulesets[i].id);

      EXPECT_EQ(info[i].manifest_id, rulesets[i].manifest_id);

      // Compare normalized FilePaths instead of their string representations
      // since different platforms represent path separators differently.
      EXPECT_EQ(base::FilePath()
                    .AppendASCII(info[i].relative_file_path)
                    .NormalizePathSeparators(),
                rulesets[i].relative_path);

      EXPECT_EQ(info[i].enabled, rulesets[i].enabled);

      EXPECT_EQ(rulesets[i].manifest_id,
                DNRManifestData::GetManifestID(*extension, rulesets[i].id));
    }
  }

  void WriteManifestAndRuleset(const base::Value& manifest,
                               const std::vector<TestRulesetInfo>& info) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    for (const auto& ruleset : info) {
      base::FilePath rules_path =
          temp_dir_.GetPath().AppendASCII(ruleset.relative_file_path);

      // Create parent directory of |rules_path| if it doesn't exist.
      EXPECT_TRUE(base::CreateDirectory(rules_path.DirName()));

      // Persist an empty ruleset file.
      EXPECT_EQ(0, base::WriteFile(rules_path, nullptr /*data*/, 0 /*size*/));
    }

    // Persist manifest file.
    JSONFileValueSerializer(temp_dir_.GetPath().Append(kManifestFilename))
        .Serialize(manifest);
  }

  TestRulesetInfo CreateDefaultRuleset() {
    return TestRulesetInfo("id", "rules_file.json", base::ListValue());
  }

 private:
  base::ScopedTempDir temp_dir_;
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(DNRManifestTest);
};

TEST_F(DNRManifestTest, EmptyRuleset) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, InvalidManifestKey) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  std::unique_ptr<base::DictionaryValue> manifest = CreateManifest(rulesets);
  manifest->SetInteger(keys::kDeclarativeNetRequestKey, 3);

  WriteManifestAndRuleset(*manifest, rulesets);
  LoadAndExpectError(
      ErrorUtils::FormatErrorMessage(errors::kInvalidDeclarativeNetRequestKey,
                                     keys::kDeclarativeNetRequestKey));
}

TEST_F(DNRManifestTest, InvalidRulesFileKey) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  std::unique_ptr<base::DictionaryValue> manifest = CreateManifest(rulesets);
  manifest->SetInteger(GetRuleResourcesKey(), 3);

  WriteManifestAndRuleset(*manifest, rulesets);
  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidDeclarativeRulesFileKey, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey));
}

TEST_F(DNRManifestTest, ZeroRulesets) {
  std::vector<TestRulesetInfo> no_rulesets;
  WriteManifestAndRuleset(*CreateManifest(no_rulesets), no_rulesets);
  LoadAndExpectSuccess(no_rulesets);
}

TEST_F(DNRManifestTest, MultipleRulesFileSuccess) {
  TestRulesetInfo ruleset_1("1", "file1.json", base::ListValue(),
                            true /* enabled */);
  TestRulesetInfo ruleset_2("2", "file2.json", base::ListValue(),
                            false /* enabled */);
  TestRulesetInfo ruleset_3("3", "file3.json", base::ListValue(),
                            true /* enabled */);

  std::vector<TestRulesetInfo> rulesets = {ruleset_1, ruleset_2, ruleset_3};
  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, MultipleRulesFileInvalidPath) {
  TestRulesetInfo ruleset_1("1", "file1.json", base::ListValue());
  TestRulesetInfo ruleset_2("2", "file2.json", base::ListValue());

  // Only persist |ruleset_1| on disk but include both in the manifest.
  WriteManifestAndRuleset(*CreateManifest({ruleset_1, ruleset_2}), {ruleset_1});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, ruleset_2.relative_file_path));
}

TEST_F(DNRManifestTest, RulesetCountExceeded) {
  std::vector<TestRulesetInfo> rulesets;
  for (int i = 0; i <= dnr_api::MAX_NUMBER_OF_STATIC_RULESETS; ++i)
    rulesets.emplace_back(base::NumberToString(i), base::ListValue());

  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesetCountExceeded, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey,
      base::NumberToString(dnr_api::MAX_NUMBER_OF_STATIC_RULESETS)));
}

TEST_F(DNRManifestTest, NonExistentRulesFile) {
  TestRulesetInfo ruleset("id", "invalid_file.json", base::ListValue());

  std::unique_ptr<base::DictionaryValue> manifest = CreateManifest({ruleset});

  WriteManifestAndRuleset(*manifest, {});

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kRulesFileIsInvalid, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, ruleset.relative_file_path));
}

TEST_F(DNRManifestTest, NeedsDeclarativeNetRequestPermission) {
  std::vector<TestRulesetInfo> rulesets({CreateDefaultRuleset()});
  std::unique_ptr<base::DictionaryValue> manifest = CreateManifest(rulesets);
  // Remove "declarativeNetRequest" permission.
  manifest->Remove(keys::kPermissions, nullptr);

  WriteManifestAndRuleset(*manifest, rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kDeclarativeNetRequestPermissionNeeded, kAPIPermission,
      keys::kDeclarativeNetRequestKey));
}

TEST_F(DNRManifestTest, RulesFileInNestedDirectory) {
  TestRulesetInfo ruleset("id", "dir/rules_file.json", base::ListValue());

  std::vector<TestRulesetInfo> rulesets({ruleset});
  std::unique_ptr<base::DictionaryValue> manifest = CreateManifest(rulesets);

  WriteManifestAndRuleset(*manifest, rulesets);
  LoadAndExpectSuccess(rulesets);
}

TEST_F(DNRManifestTest, EmptyRulesetID) {
  TestRulesetInfo ruleset_1("1", "1.json", base::ListValue());
  TestRulesetInfo ruleset_2("", "2.json", base::ListValue());
  TestRulesetInfo ruleset_3("3", "3.json", base::ListValue());
  std::vector<TestRulesetInfo> rulesets({ruleset_1, ruleset_2, ruleset_3});
  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, "1"));
}

TEST_F(DNRManifestTest, DuplicateRulesetID) {
  TestRulesetInfo ruleset_1("1", "1.json", base::ListValue());
  TestRulesetInfo ruleset_2("2", "2.json", base::ListValue());
  TestRulesetInfo ruleset_3("3", "3.json", base::ListValue());
  TestRulesetInfo ruleset_4("1", "3.json", base::ListValue());
  std::vector<TestRulesetInfo> rulesets(
      {ruleset_1, ruleset_2, ruleset_3, ruleset_4});
  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, "3"));
}

TEST_F(DNRManifestTest, ReservedRulesetID) {
  TestRulesetInfo ruleset_1("foo", "1.json", base::ListValue());
  TestRulesetInfo ruleset_2("_bar", "2.json", base::ListValue());
  TestRulesetInfo ruleset_3("baz", "3.json", base::ListValue());
  std::vector<TestRulesetInfo> rulesets({ruleset_1, ruleset_2, ruleset_3});
  WriteManifestAndRuleset(*CreateManifest(rulesets), rulesets);

  LoadAndExpectError(ErrorUtils::FormatErrorMessage(
      errors::kInvalidRulesetID, keys::kDeclarativeNetRequestKey,
      keys::kDeclarativeRuleResourcesKey, "1"));
}

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
