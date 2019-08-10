// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/about_flags.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/flags_ui/feature_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace about_flags {

namespace {

using Sample = base::HistogramBase::Sample;
using SwitchToIdMap = std::map<std::string, Sample>;

// Get all associated switches corresponding to defined about_flags.cc entries.
std::set<std::string> GetAllSwitchesAndFeaturesForTesting() {
  std::set<std::string> result;

  size_t num_entries = 0;
  const flags_ui::FeatureEntry* entries =
      testing::GetFeatureEntries(&num_entries);

  for (size_t i = 0; i < num_entries; ++i) {
    const flags_ui::FeatureEntry& entry = entries[i];
    switch (entry.type) {
      case flags_ui::FeatureEntry::SINGLE_VALUE:
      case flags_ui::FeatureEntry::SINGLE_DISABLE_VALUE:
        result.insert(entry.command_line_switch);
        break;
      case flags_ui::FeatureEntry::ORIGIN_LIST_VALUE:
        // Do nothing, origin list values are not added as feature flags.
        break;
      case flags_ui::FeatureEntry::MULTI_VALUE:
        for (int j = 0; j < entry.num_options; ++j) {
          result.insert(entry.ChoiceForOption(j).command_line_switch);
        }
        break;
      case flags_ui::FeatureEntry::ENABLE_DISABLE_VALUE:
        result.insert(entry.command_line_switch);
        result.insert(entry.disable_command_line_switch);
        break;
      case flags_ui::FeatureEntry::FEATURE_VALUE:
      case flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
        result.insert(std::string(entry.feature->name) + ":enabled");
        result.insert(std::string(entry.feature->name) + ":disabled");
        break;
    }
  }
  return result;
}

enum class FlagFile { kFlagMetadata, kFlagNeverExpire };

std::string FlagFileName(FlagFile file) {
  switch (file) {
    case FlagFile::kFlagMetadata:
      return "flag-metadata.json";
    case FlagFile::kFlagNeverExpire:
      return "flag-never-expire-list.json";
  }
}

base::Value FileContents(FlagFile file) {
  std::string filename = FlagFileName(file);

  base::FilePath metadata_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &metadata_path);
  JSONFileValueDeserializer deserializer(
      metadata_path.AppendASCII("chrome").AppendASCII("browser").AppendASCII(
          filename));
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> json =
      deserializer.Deserialize(&error_code, &error_message);
  DCHECK(json) << "Failed to load " << filename << ": " << error_code << " "
               << error_message;
  return std::move(*json);
}

struct FlagMetadataEntry {
  std::vector<std::string> owners;
  int expiry_milestone;
};

using FlagMetadataMap = std::map<std::string, FlagMetadataEntry>;

FlagMetadataMap LoadFlagMetadata() {
  base::Value metadata_json = FileContents(FlagFile::kFlagMetadata);

  FlagMetadataMap metadata;
  for (const auto& entry : metadata_json.GetList()) {
    std::string name = entry.FindKey("name")->GetString();
    std::vector<std::string> owners;
    if (const base::Value* e = entry.FindKey("owners")) {
      for (const auto& owner : e->GetList())
        owners.push_back(owner.GetString());
    }
    int expiry_milestone = entry.FindKey("expiry_milestone")->GetInt();
    metadata[name] = FlagMetadataEntry{owners, expiry_milestone};
  }

  return metadata;
}

std::vector<std::string> LoadFlagNeverExpireList() {
  base::Value list_json = FileContents(FlagFile::kFlagNeverExpire);

  std::vector<std::string> result;
  for (const auto& entry : list_json.GetList()) {
    result.push_back(entry.GetString());
  }
  return result;
}

}  // anonymous namespace

// Makes sure there are no separators in any of the entry names.
TEST(AboutFlagsTest, NoSeparators) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  for (size_t i = 0; i < count; ++i) {
    std::string name = entries[i].internal_name;
    EXPECT_EQ(std::string::npos, name.find(flags_ui::testing::kMultiSeparator))
        << i;
  }
}

// Makes sure that every flag has an owner and an expiry entry in
// flag-metadata.json.
TEST(AboutFlagsTest, EveryFlagHasMetadata) {
  size_t count;
  const flags_ui::FeatureEntry* entries = testing::GetFeatureEntries(&count);
  FlagMetadataMap metadata = LoadFlagMetadata();

  std::vector<std::string> missing_flags;

  for (size_t i = 0; i < count; ++i) {
    if (metadata.count(entries[i].internal_name) == 0)
      missing_flags.push_back(entries[i].internal_name);
  }

  std::sort(missing_flags.begin(), missing_flags.end());

  EXPECT_EQ(0u, missing_flags.size())
      << "Missing flags: " << base::JoinString(missing_flags, "\n  ");
}

TEST(AboutFlagsTest, OnlyPermittedFlagsNeverExpire) {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> listed_flags = LoadFlagNeverExpireList();
  std::vector<std::string> missing_flags;

  for (const auto& entry : metadata) {
    if (entry.second.expiry_milestone == -1 &&
        std::find(listed_flags.begin(), listed_flags.end(), entry.first) ==
            listed_flags.end()) {
      missing_flags.push_back(entry.first);
    }
  }

  std::sort(missing_flags.begin(), missing_flags.end());

  EXPECT_EQ(0u, missing_flags.size())
      << "Flags not listed for no-expire: "
      << base::JoinString(missing_flags, "\n  ");
}

TEST(AboutFlagsTest, EveryFlagHasNonEmptyOwners) {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> sad_flags;

  for (const auto& it : metadata) {
    if (it.second.owners.empty())
      sad_flags.push_back(it.first);
  }

  std::sort(sad_flags.begin(), sad_flags.end());

  EXPECT_EQ(0u, sad_flags.size())
      << "Flags missing owners: " << base::JoinString(sad_flags, "\n  ");
}

namespace {

void EnsureNamesAreAlphabetical(
    const std::vector<std::string>& normalized_names,
    const std::vector<std::string>& names,
    FlagFile file) {
  if (normalized_names.size() < 2)
    return;

  for (size_t i = 1; i < normalized_names.size(); ++i) {
    if (i == normalized_names.size() - 1) {
      // The last item on the list has less context.
      EXPECT_TRUE(normalized_names[i - 1] < normalized_names[i])
          << "Correct alphabetical order does not place '" << names[i]
          << "' after '" << names[i - 1] << "' in " << FlagFileName(file);
    } else {
      EXPECT_TRUE(normalized_names[i - 1] < normalized_names[i] &&
                  normalized_names[i] < normalized_names[i + 1])
          << "Correct alphabetical order does not place '" << names[i]
          << "' between '" << names[i - 1] << "' and '" << names[i + 1]
          << "' in " << FlagFileName(file);
    }
  }
}

std::string NormalizeName(const std::string& name) {
  std::string normalized_name = base::ToLowerASCII(name);
  std::replace(normalized_name.begin(), normalized_name.end(), '_', '-');

  return normalized_name;
}

}  // namespace

// For some bizarre reason, far too many people see a file filled with
// alphabetically-ordered items and think "hey, let me drop this new item into a
// random location!" Prohibit such behavior in the flags files.
TEST(AboutFlagsTest, FlagsListedInAlphabeticalOrder) {
  base::Value metadata_json = FileContents(FlagFile::kFlagMetadata);

  std::vector<std::string> normalized_names;
  std::vector<std::string> names;
  for (const auto& entry : metadata_json.GetList()) {
    normalized_names.push_back(
        NormalizeName(entry.FindKey("name")->GetString()));
    names.push_back(entry.FindKey("name")->GetString());
  }

  EnsureNamesAreAlphabetical(normalized_names, names, FlagFile::kFlagMetadata);

  base::Value expiration_json = FileContents(FlagFile::kFlagNeverExpire);

  normalized_names.clear();
  names.clear();
  for (const auto& entry : expiration_json.GetList()) {
    normalized_names.push_back(NormalizeName(entry.GetString()));
    names.push_back(entry.GetString());
  }

  EnsureNamesAreAlphabetical(normalized_names, names,
                             FlagFile::kFlagNeverExpire);
}

class AboutFlagsHistogramTest : public ::testing::Test {
 protected:
  // This is a helper function to check that all IDs in enum LoginCustomFlags in
  // histograms.xml are unique.
  void SetSwitchToHistogramIdMapping(const std::string& switch_name,
                                     const Sample switch_histogram_id,
                                     std::map<std::string, Sample>* out_map) {
    const std::pair<std::map<std::string, Sample>::iterator, bool> status =
        out_map->insert(std::make_pair(switch_name, switch_histogram_id));
    if (!status.second) {
      EXPECT_TRUE(status.first->second == switch_histogram_id)
          << "Duplicate switch '" << switch_name
          << "' found in enum 'LoginCustomFlags' in histograms.xml.";
    }
  }

  // This method generates a hint for the user for what string should be added
  // to the enum LoginCustomFlags to make in consistent.
  std::string GetHistogramEnumEntryText(const std::string& switch_name,
                                        Sample value) {
    return base::StringPrintf(
        "<int value=\"%d\" label=\"%s\"/>", value, switch_name.c_str());
  }
};

TEST_F(AboutFlagsHistogramTest, CheckHistograms) {
  base::Optional<base::HistogramEnumEntryMap> login_custom_flags =
      base::ReadEnumFromEnumsXml("LoginCustomFlags");
  ASSERT_TRUE(login_custom_flags)
      << "Error reading enum 'LoginCustomFlags' from enums.xml.";

  // Build reverse map {switch_name => id} from login_custom_flags.
  SwitchToIdMap histograms_xml_switches_ids;

  EXPECT_TRUE(login_custom_flags->count(testing::kBadSwitchFormatHistogramId))
      << "Entry for UMA ID of incorrect command-line flag is not found in "
         "enums.xml enum LoginCustomFlags. "
         "Consider adding entry:\n"
      << "  " << GetHistogramEnumEntryText("BAD_FLAG_FORMAT", 0);
  // Check that all LoginCustomFlags entries have correct values.
  for (const auto& entry : *login_custom_flags) {
    if (entry.first == testing::kBadSwitchFormatHistogramId) {
      // Add error value with empty name.
      SetSwitchToHistogramIdMapping(std::string(), entry.first,
                                    &histograms_xml_switches_ids);
      continue;
    }
    const Sample uma_id = GetSwitchUMAId(entry.second);
    EXPECT_EQ(uma_id, entry.first)
        << "enums.xml enum LoginCustomFlags "
           "entry '"
        << entry.second << "' has incorrect value=" << entry.first << ", but "
        << uma_id << " is expected. Consider changing entry to:\n"
        << "  " << GetHistogramEnumEntryText(entry.second, uma_id);
    SetSwitchToHistogramIdMapping(entry.second, entry.first,
                                  &histograms_xml_switches_ids);
  }

  // Check that all flags in about_flags.cc have entries in login_custom_flags.
  std::set<std::string> all_flags = GetAllSwitchesAndFeaturesForTesting();
  for (const std::string& flag : all_flags) {
    // Skip empty placeholders.
    if (flag.empty())
      continue;
    const Sample uma_id = GetSwitchUMAId(flag);
    EXPECT_NE(testing::kBadSwitchFormatHistogramId, uma_id)
        << "Command-line switch '" << flag
        << "' from about_flags.cc has UMA ID equal to reserved value "
           "kBadSwitchFormatHistogramId="
        << testing::kBadSwitchFormatHistogramId
        << ". Please modify switch name.";
    auto enum_entry = histograms_xml_switches_ids.lower_bound(flag);

    // Ignore case here when switch ID is incorrect - it has already been
    // reported in the previous loop.
    EXPECT_TRUE(enum_entry != histograms_xml_switches_ids.end() &&
                enum_entry->first == flag)
        << "enums.xml enum LoginCustomFlags doesn't contain switch '" << flag
        << "' (value=" << uma_id << " expected). Consider adding entry:\n"
        << "  " << GetHistogramEnumEntryText(flag, uma_id);
  }
}

}  // namespace about_flags
