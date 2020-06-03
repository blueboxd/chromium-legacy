// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_result.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"

namespace app_list {

AnswerCardSearchProvider::AnswerCardSearchProvider(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller)
    : profile_(profile),
      model_updater_(model_updater),
      list_controller_(list_controller),
      answer_server_url_(app_list_features::AnswerServerUrl()),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile)) {
}

AnswerCardSearchProvider::~AnswerCardSearchProvider() = default;

ash::AppListSearchResultType AnswerCardSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kAnswerCard;
}

void AnswerCardSearchProvider::Start(const base::string16& query) {
  if (!model_updater_->SearchEngineIsGoogle()) {
    UpdateQuery(base::string16());
    return;
  }

  UpdateQuery(query);
}

void AnswerCardSearchProvider::UpdateQuery(const base::string16& query) {
  if (query.empty()) {
    ClearResults();
    current_potential_answer_card_url_ = GURL();
    return;
  }

  const std::string prefixed_query(
      "q=" + net::EscapeQueryParamValue(base::UTF16ToUTF8(query), true) +
      app_list_features::AnswerServerQuerySuffix());
  GURL::Replacements replacements;
  replacements.SetQueryStr(prefixed_query);
  GURL potential_answer_card_url =
      answer_server_url_.ReplaceComponents(replacements);
  if (potential_answer_card_url == current_potential_answer_card_url_)
    return;

  current_potential_answer_card_url_ = potential_answer_card_url;
  GURL search_result_url =
      GURL(template_url_service_->GetDefaultSearchProvider()
               ->url_ref()
               .ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(query),
                                   template_url_service_->search_terms_data()));
  const GURL stripped_search_result_url = AutocompleteMatch::GURLToStrippedGURL(
      GURL(search_result_url), AutocompleteInput(), template_url_service_,
      base::string16() /* keyword */);

  SearchProvider::Results results;
  results.emplace_back(std::make_unique<AnswerCardResult>(
      profile_, list_controller_, potential_answer_card_url, search_result_url,
      stripped_search_result_url));
  SwapResults(&results);
}

}  // namespace app_list
