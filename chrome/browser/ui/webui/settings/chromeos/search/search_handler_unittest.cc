// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/fake_hierarchy.h"
#include "chrome/browser/ui/webui/settings/chromeos/fake_os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom-test-utils.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPrintingDetails},
       {IDS_OS_SETTINGS_TAG_PRINTING_ALT1, IDS_OS_SETTINGS_TAG_PRINTING_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

class SearchHandlerTest : public testing::Test {
 protected:
  SearchHandlerTest()
      : search_tag_registry_(&local_search_service_),
        fake_hierarchy_(&fake_sections_),
        handler_(&search_tag_registry_,
                 &fake_sections_,
                 &fake_hierarchy_,
                 &local_search_service_) {}
  ~SearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kNewOsSettingsSearch);
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    fake_hierarchy_.AddSubpageMetadata(
        IDS_SETTINGS_PRINTING_CUPS_PRINTERS, mojom::Section::kPrinting,
        mojom::Subpage::kPrintingDetails, mojom::SearchResultIcon::kPrinter,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::kPrintingDetailsSubpagePath);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPrinting,
                                       mojom::Setting::kAddPrinter);
    fake_hierarchy_.AddSettingMetadata(mojom::Section::kPrinting,
                                       mojom::Setting::kSavedPrinters);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  local_search_service::LocalSearchService local_search_service_;
  SearchTagRegistry search_tag_registry_;
  FakeOsSettingsSections fake_sections_;
  FakeHierarchy fake_hierarchy_;
  SearchHandler handler_;
  mojo::Remote<mojom::SearchHandler> handler_remote_;
};

TEST_F(SearchHandlerTest, AddAndRemove) {
  // Add printing search tags to registry and search for "Printing".
  search_tag_registry_.AddSearchTags(GetPrintingSearchConcepts());
  std::vector<mojom::SearchResultPtr> search_results;

  // 2 results should be available for a "Printing" query.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Printing"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kDoNotIncludeParentResults,
              &search_results);
  EXPECT_EQ(search_results.size(), 2u);

  // Limit results to 1 max and ensure that only 1 result is returned.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Printing"),
              /*max_num_results=*/1u,
              mojom::ParentResultBehavior::kDoNotIncludeParentResults,
              &search_results);
  EXPECT_EQ(search_results.size(), 1u);

  // Search for a query which should return no results.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("QueryWithNoResults"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kDoNotIncludeParentResults,
              &search_results);
  EXPECT_TRUE(search_results.empty());

  // Remove printing search tags to registry and verify that no results are
  // returned for "Printing".
  search_tag_registry_.RemoveSearchTags(GetPrintingSearchConcepts());
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Printing"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kDoNotIncludeParentResults,
              &search_results);
  EXPECT_TRUE(search_results.empty());
}

TEST_F(SearchHandlerTest, UrlModification) {
  // Add printing search tags to registry and search for "Saved".
  search_tag_registry_.AddSearchTags(GetPrintingSearchConcepts());
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Saved"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kDoNotIncludeParentResults,
              &search_results);

  // Only the "saved printers" item should be returned.
  EXPECT_EQ(search_results.size(), 1u);

  // The URL should have bee modified according to the FakeOsSettingSection
  // scheme.
  EXPECT_EQ(
      std::string("Section::kPrinting::") + mojom::kPrintingDetailsSubpagePath,
      search_results[0]->url_path_with_parameters);
}

TEST_F(SearchHandlerTest, AllowParentResult) {
  // Add printing search tags to registry.
  search_tag_registry_.AddSearchTags(GetPrintingSearchConcepts());
  std::vector<mojom::SearchResultPtr> search_results;

  // Search for "Saved", which should only apply to the "saved printers" item.
  // Pass the kAllowParentResults flag, which should also cause its parent
  // subpage item to be returned.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Saved"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kAllowParentResults,
              &search_results);
  EXPECT_EQ(search_results.size(), 2u);
}

TEST_F(SearchHandlerTest, DefaultRank) {
  // Add printing search tags to registry.
  search_tag_registry_.AddSearchTags(GetPrintingSearchConcepts());
  std::vector<mojom::SearchResultPtr> search_results;

  // Search for "Printing". Only the IDS_OS_SETTINGS_TAG_PRINTING result
  // contains the word "Printing", but the other results have the similar word
  // "Printer". Thus, "Printing" has a higher relevance score.
  mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(base::ASCIIToUTF16("Printing"),
              /*max_num_results=*/3u,
              mojom::ParentResultBehavior::kAllowParentResults,
              &search_results);
  EXPECT_EQ(search_results.size(), 3u);

  // Since the IDS_OS_SETTINGS_TAG_PRINTING result has a default rank of kLow,
  // it should be the *last* result returned even though it has a higher
  // relevance score.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OS_SETTINGS_TAG_PRINTING),
            search_results[2]->result_text);
}

}  // namespace settings
}  // namespace chromeos
