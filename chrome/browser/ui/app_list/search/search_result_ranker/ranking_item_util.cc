// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match_type.h"

namespace app_list {

// Given a ChromeSearchResult representing an omnibox result, convert it based
// on the subtype specified by ChromeSearchResult::result_subtype. Any
// unanticipated subtypes are converted into RankingItemType::kOmniboxGeneric.
RankingItemType ExpandOmniboxType(const ChromeSearchResult& result) {
  if (result.result_type() != ash::SearchResultType::kOmnibox) {
    NOTREACHED();
    return RankingItemType::kUnknown;
  }

  switch (static_cast<AutocompleteMatchType::Type>(result.result_subtype())) {
    case AutocompleteMatchType::Type::HISTORY_URL:
    case AutocompleteMatchType::Type::HISTORY_TITLE:
    case AutocompleteMatchType::Type::HISTORY_BODY:
    case AutocompleteMatchType::Type::HISTORY_KEYWORD:
      return RankingItemType::kOmniboxHistory;
    case AutocompleteMatchType::Type::NAVSUGGEST:
    case AutocompleteMatchType::Type::NAVSUGGEST_PERSONALIZED:
      return RankingItemType::kOmniboxNavSuggest;
    case AutocompleteMatchType::Type::SEARCH_HISTORY:
    case AutocompleteMatchType::Type::SEARCH_SUGGEST:
    case AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::Type::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::Type::SEARCH_SUGGEST_PERSONALIZED:
    case AutocompleteMatchType::Type::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::Type::SEARCH_OTHER_ENGINE:
      return RankingItemType::kOmniboxSearch;
    case AutocompleteMatchType::Type::BOOKMARK_TITLE:
      return RankingItemType::kOmniboxBookmark;
    case AutocompleteMatchType::Type::DOCUMENT_SUGGESTION:
      return RankingItemType::kOmniboxDocument;
    case AutocompleteMatchType::Type::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::Type::CONTACT_DEPRECATED:
    case AutocompleteMatchType::Type::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::Type::TAB_SEARCH_DEPRECATED:
      return RankingItemType::kOmniboxDeprecated;
    default:
      return RankingItemType::kOmniboxGeneric;
  }
}

RankingItemType RankingItemTypeFromSearchResult(
    const ChromeSearchResult& result) {
  // We don't want or expect the expand_omnibox_types parameter to change during
  // the execution of chrome, so make it static.
  static bool expand_omnibox_types = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kEnableQueryBasedMixedTypesRanker,
      "expand_omnibox_types", false);

  switch (result.result_type()) {
    case ash::SearchResultType::kInstalledApp:
    case ash::SearchResultType::kInternalApp:
      return RankingItemType::kApp;
    case ash::SearchResultType::kOmnibox:
      if (expand_omnibox_types)
        return ExpandOmniboxType(result);
      return RankingItemType::kOmniboxGeneric;
    case ash::SearchResultType::kLauncher:
      return RankingItemType::kFile;
    case ash::SearchResultType::kUnknown:
    case ash::SearchResultType::kPlayStoreApp:
    case ash::SearchResultType::kInstantApp:
    case ash::SearchResultType::kAnswerCard:
    case ash::SearchResultType::kPlayStoreReinstallApp:
      return RankingItemType::kIgnored;
    case ash::SearchResultType::kArcAppShortcut:
      return RankingItemType::kArcAppShortcut;
  }
}

RankingItemType RankingItemTypeFromChromeAppListItem(
    const ChromeAppListItem& item) {
  return RankingItemType::kApp;
}

std::string SimplifyUrlId(const std::string& url_id) {
  std::string result(url_id);

  std::size_t query_index = result.find("?");
  if (query_index != std::string::npos)
    result.erase(query_index);

  std::size_t fragment_index = result.find("#");
  if (fragment_index != std::string::npos)
    result.erase(fragment_index);

  if (!result.empty() && result.back() == '/' &&
      result.find("://") != result.size() - 3)
    result.pop_back();

  return result;
}

std::string SimplifyGoogleDocsUrlId(const std::string& url_id) {
  std::string result = SimplifyUrlId(url_id);

  // URLs that end with /view or /edit point to the same document, so should be
  // the same for ranking purposes.
  if (base::EndsWith(result, "/view", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(result, "/edit", base::CompareCase::INSENSITIVE_ASCII)) {
    result = result.substr(0, result.length() - 5);
  }

  return result;
}

}  // namespace app_list
