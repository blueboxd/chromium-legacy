// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/local_history_zero_suggest_provider.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

namespace {

// Default relevance for the LocalHistoryZeroSuggestProvider query suggestions.
const int kLocalHistoryZeroSuggestRelevance = 500;

// ProviderHistoryDBTask wraps two non-null callbacks into a HistoryDBTask to
// passed to HistoryService::ScheduleDBTask. |request_callback| gets invoked on
// the history backend thread when the on-disk history database becomes
// available in order to query the database and populate |url_matches_|.
// |result_callback_| is then invoked with |url_matches_| back on the main
// thread.
class ProviderHistoryDBTask : public history::HistoryDBTask {
 public:
  using RequestCallback =
      base::OnceCallback<history::URLRows(history::URLDatabase* url_db)>;
  using ResultCallback = base::OnceCallback<void(const history::URLRows&)>;

  ProviderHistoryDBTask(RequestCallback request_callback,
                        ResultCallback result_callback)
      : request_callback_(std::move(request_callback)),
        result_callback_(std::move(result_callback)) {
    DCHECK(!request_callback_.is_null());
    DCHECK(!result_callback_.is_null());
  }

  ~ProviderHistoryDBTask() override {}

 private:
  // history::HistoryDBTask
  bool RunOnDBThread(history::HistoryBackend* history_backend,
                     history::HistoryDatabase* db) override {
    // Query the database.
    if (db)
      url_matches_ = std::move(request_callback_).Run(db);
    return true;
  }

  void DoneRunOnMainThread() override {
    // Return the query results to the originating thread.
    std::move(result_callback_).Run(url_matches_);
  }

  RequestCallback request_callback_;
  ResultCallback result_callback_;
  history::URLRows url_matches_;

  DISALLOW_COPY_AND_ASSIGN(ProviderHistoryDBTask);
};

// Queries |url_db| for URLs matching |google_search_url| prefix and returns
// the results. |url_db| must not be null. May be called on the history backend
// thread.
history::URLRows QueryURLDatabaseOnHistoryThread(
    const std::string& google_search_url,
    const size_t provider_max_matches,
    history::URLDatabase* url_db) {
  DCHECK(url_db);

  // Request 5x more URLs than the number of matches the provider intends to
  // return hoping to have enough URLs to work with once duplicates are filtered
  // out.
  history::URLRows url_matches;
  url_db->AutocompleteForPrefix(google_search_url, 5 * provider_max_matches,
                                false, &url_matches);
  return url_matches;
}

// Extracts the search terms from |url|. Collapses whitespaces, converts them to
// lowercase and returns them. |template_url_service| must not be null.
base::string16 GetSearchTermsFromURL(const GURL& url,
                                     TemplateURLService* template_url_service) {
  DCHECK(template_url_service);
  base::string16 search_terms;
  template_url_service->GetDefaultSearchProvider()->ExtractSearchTermsFromURL(
      url, template_url_service->search_terms_data(), &search_terms);
  return base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
}

}  // namespace

// static
const char LocalHistoryZeroSuggestProvider::kZeroSuggestLocalVariant[] =
    "Local";

// static
LocalHistoryZeroSuggestProvider* LocalHistoryZeroSuggestProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new LocalHistoryZeroSuggestProvider(client, listener);
}

void LocalHistoryZeroSuggestProvider::Start(const AutocompleteInput& input,
                                            bool minimal_changes) {
  TRACE_EVENT0("omnibox", "LocalHistoryZeroSuggestProvider::Start");

  history_task_tracker_.TryCancel(history_db_task_id_);
  done_ = true;
  matches_.clear();

  // Allow local history query suggestions only when the user is unauthenticated
  // and is not in an off-the-record context.
  if (client_->IsAuthenticated() || client_->IsOffTheRecord())
    return;

  // Allow local history query suggestions only when the omnibox is empty and is
  // focused from the NTP.
  if (!input.from_omnibox_focus() ||
      input.type() != metrics::OmniboxInputType::EMPTY ||
      !BaseSearchProvider::IsNTPPage(input.current_page_classification())) {
    return;
  }

  // Allow local history query suggestions only when the user has set up Google
  // as their default search engine.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider() ||
      template_url_service->GetDefaultSearchProvider()->GetEngineType(
          template_url_service->search_terms_data()) != SEARCH_ENGINE_GOOGLE) {
    return;
  }

  if (!base::Contains(OmniboxFieldTrial::GetZeroSuggestVariants(
                          input.current_page_classification()),
                      kZeroSuggestLocalVariant)) {
    return;
  }

  history::HistoryService* const history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  GURL google_base_url(
      template_url_service->search_terms_data().GoogleBaseURLValue());
  std::string google_search_url =
      google_util::GetGoogleSearchURL(google_base_url).spec();

  // Query the in-memory URL database, if available. Otherwise, schedule a
  // task to query the on-disk history database on the history backend thread.
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (url_db) {
    const auto url_matches = QueryURLDatabaseOnHistoryThread(
        google_search_url, max_matches_, url_db);
    OnQueryURLDatabaseComplete(input, std::move(url_matches));
  } else {
    done_ = false;
    history_db_task_id_ = history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<ProviderHistoryDBTask>(
            base::BindOnce(QueryURLDatabaseOnHistoryThread, google_search_url,
                           max_matches_),
            base::BindOnce(
                &LocalHistoryZeroSuggestProvider::OnQueryURLDatabaseComplete,
                weak_ptr_factory_.GetWeakPtr(), input)),
        &history_task_tracker_);
  }
}

void LocalHistoryZeroSuggestProvider::DeleteMatch(
    const AutocompleteMatch& match) {
  history::HistoryService* history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // Generate a Google search URL for the suggestion by replacing the search
  // terms in the Google search URL template. Note that the suggestion's
  // original search URL cannot be used here as it is unique due to the assisted
  // query stats (aka AQS) query param which contains impressions of
  // autocomplete matches shown at query submission time.
  GURL google_search_url;
  TemplateURLRef::SearchTermsArgs search_terms_args(match.contents);
  const auto* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const auto& search_terms_data = template_url_service->search_terms_data();
  default_search_provider->ReplaceSearchTermsInURL(
      default_search_provider->GenerateSearchURL(search_terms_data),
      search_terms_args, search_terms_data, &google_search_url);

  // Query the HistoryService for URLs matching the Google search URL. Note that
  // this step is necessary here because 1) there may be matching fresh URLs
  // that were not encountered in the suggestion creation phase due to looking
  // up a maximum number of URLs in that phase. and 2) the performance overhead
  // of requerying the HistoryService cannot be tolerated in the suggestion
  // creation phase. Here on the other hand it can due to the small percentage
  // of suggestions getting deleted relative to the number of suggestions shown.
  history::QueryOptions opts;
  opts.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  opts.begin_time = history::AutocompleteAgeThreshold();
  history_service->QueryHistory(
      base::ASCIIToUTF16(google_search_url.spec()), opts,
      base::BindOnce(&LocalHistoryZeroSuggestProvider::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), match.contents),
      &history_task_tracker_);

  // Prevent the deleted suggestion from being resuggested until the
  // corresponding URLs are asynchronously deleted.
  deleted_suggestions_set_.insert(match.contents);
}

LocalHistoryZeroSuggestProvider::LocalHistoryZeroSuggestProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY),
      max_matches_(AutocompleteResult::GetMaxMatches(/*is_zero_suggest=*/true)),
      client_(client),
      listener_(listener) {}

LocalHistoryZeroSuggestProvider::~LocalHistoryZeroSuggestProvider() {}

void LocalHistoryZeroSuggestProvider::OnQueryURLDatabaseComplete(
    const AutocompleteInput& input,
    const history::URLRows& url_matches) {
  done_ = true;
  matches_.clear();

  // Fail if we can't extract the search terms from the URL matches.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // Used to filter out duplicate query suggestions.
  std::set<base::string16> seen_suggestions_set;

  int relevance = kLocalHistoryZeroSuggestRelevance;
  for (size_t i = 0; i < url_matches.size(); i++) {
    const history::URLRow& url_row = url_matches[i];
    const GURL& url = url_row.url();

    // Discard the URL if it is not valid or fresh enough.
    if (!url.is_valid() ||
        url_row.last_visit() < history::AutocompleteAgeThreshold()) {
      continue;
    }

    base::string16 search_terms =
        GetSearchTermsFromURL(url, template_url_service);
    if (search_terms.empty())
      continue;

    // Filter out duplicate query suggestions.
    if (seen_suggestions_set.count(search_terms))
      continue;
    seen_suggestions_set.insert(search_terms);

    // Filter out deleted query suggestions.
    if (deleted_suggestions_set_.count(search_terms))
      continue;

    SearchSuggestionParser::SuggestResult suggestion(
        /*suggestion=*/search_terms, AutocompleteMatchType::SEARCH_HISTORY,
        /*subtype_identifier=*/0, /*from_keyword=*/false, relevance--,
        /*relevance_from_server=*/0,
        /*input_text=*/base::ASCIIToUTF16(std::string()));

    AutocompleteMatch match = BaseSearchProvider::CreateSearchSuggestion(
        this, input, /*in_keyword_mode=*/false, suggestion,
        template_url_service->GetDefaultSearchProvider(),
        template_url_service->search_terms_data(),
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
        /*append_extra_query_params_from_command_line*/ true);
    match.deletable = true;

    matches_.push_back(match);
    if (matches_.size() >= max_matches_)
      break;
  }

  listener_->OnProviderUpdate(true);
}

void LocalHistoryZeroSuggestProvider::OnHistoryQueryResults(
    const base::string16& suggestion,
    history::QueryResults results) {
  history::HistoryService* history_service = client_->GetHistoryService();
  if (!history_service)
    return;

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  // Delete the matching URLs that would generate |suggestion|.
  std::vector<GURL> urls_to_delete;
  for (const auto& result : results) {
    base::string16 search_terms =
        GetSearchTermsFromURL(result.url(), template_url_service);
    if (search_terms == suggestion)
      urls_to_delete.push_back(result.url());
  }
  history_service->DeleteURLs(urls_to_delete);
}
