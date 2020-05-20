// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/local_search_service/index.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {
namespace {

// Note: Copied from printing_section.cc but does not need to stay in sync with
// it.
const std::vector<SearchConcept>& GetPrintingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddPrinter},
       {IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER_ALT1,
        IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PRINTING_SAVED_PRINTERS,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSavedPrinters}},
      {IDS_OS_SETTINGS_TAG_PRINTING,
       mojom::kPrintingDetailsSubpagePath,
       mojom::SearchResultIcon::kPrinter,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

class SearchTagRegistryTest : public testing::Test {
 protected:
  SearchTagRegistryTest() : search_tag_registry_(&local_search_service_) {}
  ~SearchTagRegistryTest() override = default;

  // testing::Test:
  void SetUp() override {
    index_ = local_search_service_.GetIndex(
        local_search_service::IndexId::kCrosSettings);
  }

  local_search_service::LocalSearchService local_search_service_;
  SearchTagRegistry search_tag_registry_;
  local_search_service::Index* index_;
};

TEST_F(SearchTagRegistryTest, AddAndRemove) {
  // Add search tags; size of the index should increase.
  search_tag_registry_.AddSearchTags(GetPrintingSearchConcepts());
  EXPECT_EQ(3u, index_->GetSize());

  // Tags added should be available via GetCanonicalTagMetadata().
  const SearchConcept* add_printer_concept =
      search_tag_registry_.GetCanonicalTagMetadata(
          IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER);
  ASSERT_TRUE(add_printer_concept);
  EXPECT_EQ(mojom::Setting::kAddPrinter, add_printer_concept->id.setting);

  // Remove search tag; size should go back to 0.
  search_tag_registry_.RemoveSearchTags(GetPrintingSearchConcepts());
  EXPECT_EQ(0u, index_->GetSize());

  // The tag should no longer be accessible via GetCanonicalTagMetadata().
  add_printer_concept = search_tag_registry_.GetCanonicalTagMetadata(
      IDS_OS_SETTINGS_TAG_PRINTING_ADD_PRINTER);
  ASSERT_FALSE(add_printer_concept);
}

}  // namespace settings
}  // namespace chromeos
