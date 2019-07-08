// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
// TODO(skare): Pull the enum in search_provider.cc into its .h file, and switch
// this file and zero_suggest_provider.cc to use it.
enum DocumentRequestsHistogramValue {
  DOCUMENT_REQUEST_SENT = 1,
  DOCUMENT_REQUEST_INVALIDATED = 2,
  DOCUMENT_REPLY_RECEIVED = 3,
  DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE
};

void LogOmniboxDocumentRequest(DocumentRequestsHistogramValue request_value) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.DocumentSuggest.Requests", request_value,
                            DOCUMENT_MAX_REQUEST_HISTOGRAM_VALUE);
}

// MIME types sent by the server for different document types.
const char kDocumentMimetype[] = "application/vnd.google-apps.document";
const char kFormMimetype[] = "application/vnd.google-apps.form";
const char kSpreadsheetMimetype[] = "application/vnd.google-apps.spreadsheet";
const char kPresentationMimetype[] = "application/vnd.google-apps.presentation";

// Returns mappings from MIME types to overridden icons.
AutocompleteMatch::DocumentType GetIconForMIMEType(
    const base::StringPiece& mimetype) {
  static const auto kIconMap =
      std::map<base::StringPiece, AutocompleteMatch::DocumentType>{
          {kDocumentMimetype, AutocompleteMatch::DocumentType::DRIVE_DOCS},
          {kFormMimetype, AutocompleteMatch::DocumentType::DRIVE_FORMS},
          {kSpreadsheetMimetype, AutocompleteMatch::DocumentType::DRIVE_SHEETS},
          {kPresentationMimetype,
           AutocompleteMatch::DocumentType::DRIVE_SLIDES},
          {"image/jpeg", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"image/png", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"image/gif", AutocompleteMatch::DocumentType::DRIVE_IMAGE},
          {"application/pdf", AutocompleteMatch::DocumentType::DRIVE_PDF},
          {"video/mp4", AutocompleteMatch::DocumentType::DRIVE_VIDEO},
      };

  const auto& iterator = kIconMap.find(mimetype);
  return iterator != kIconMap.end()
             ? iterator->second
             : AutocompleteMatch::DocumentType::DRIVE_OTHER;
}

const char kErrorMessageAdminDisabled[] =
    "Not eligible to query due to admin disabled Chrome search settings.";
const char kErrorMessageRetryLater[] = "Not eligible to query, see retry info.";
bool ResponseContainsBackoffSignal(const base::DictionaryValue* root_dict) {
  const base::DictionaryValue* error_info;
  if (!root_dict->GetDictionary("error", &error_info)) {
    return false;
  }
  int code;
  std::string status;
  std::string message;
  if (!error_info->GetInteger("code", &code) ||
      !error_info->GetString("status", &status) ||
      !error_info->GetString("message", &message)) {
    return false;
  }

  // 403/PERMISSION_DENIED: Account is currently ineligible to receive results.
  if (code == 403 && status == "PERMISSION_DENIED" &&
      message == kErrorMessageAdminDisabled) {
    return true;
  }

  // 503/UNAVAILABLE: Uninteresting set of results, or another server request to
  // backoff.
  if (code == 503 && status == "UNAVAILABLE" &&
      message == kErrorMessageRetryLater) {
    return true;
  }

  return false;
}

}  // namespace

// static
DocumentProvider* DocumentProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new DocumentProvider(client, listener);
}

// static
void DocumentProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(omnibox::kDocumentSuggestEnabled, true);
}

bool DocumentProvider::IsDocumentProviderAllowed(
    AutocompleteProviderClient* client) {
  // Feature must be on.
  if (!base::FeatureList::IsEnabled(omnibox::kDocumentProvider))
    return false;

  // These may seem like search suggestions, so gate on that setting too.
  if (!client->SearchSuggestEnabled())
    return false;

  // Client-side toggle must be enabled.
  if (!client->GetPrefs()->GetBoolean(omnibox::kDocumentSuggestEnabled))
    return false;

  // No incognito.
  if (client->IsOffTheRecord())
    return false;

  // Check sync's status and proceed if active.
  bool authenticated_and_syncing =
      client->IsAuthenticated() && client->IsSyncActive();
  if (!authenticated_and_syncing)
    return false;

  // We haven't received a server backoff signal.
  if (backoff_for_session_) {
    return false;
  }

  // Google must be set as default search provider; we mix results which may
  // change placement.
  auto* template_url_service = client->GetTemplateURLService();
  if (template_url_service == nullptr)
    return false;
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider != nullptr &&
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

// static
bool DocumentProvider::IsInputLikelyURL(const AutocompleteInput& input) {
  if (input.type() == metrics::OmniboxInputType::URL)
    return true;

  // Special cases when the user might be starting to type the most common URL
  // prefixes, but the SchemeClassifier won't have classified them as URLs yet.
  // Note these checks are of the form "(string constant) starts with input."
  if (input.text().length() <= 8) {
    if (StartsWith(base::ASCIIToUTF16("https://"), input.text(),
                   base::CompareCase::INSENSITIVE_ASCII) ||
        StartsWith(base::ASCIIToUTF16("http://"), input.text(),
                   base::CompareCase::INSENSITIVE_ASCII) ||
        StartsWith(base::ASCIIToUTF16("www."), input.text(),
                   base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  return false;
}

void DocumentProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Start");
  matches_.clear();
  field_trial_triggered_ = false;

  // Perform various checks - feature is enabled, user is allowed to use the
  // feature, we're not under backoff, etc.
  if (!IsDocumentProviderAllowed(client_)) {
    return;
  }

  // Experiment: don't issue queries for inputs under some length.
  const size_t min_query_length =
      static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
          omnibox::kDocumentProvider, "DocumentProviderMinQueryLength", 4));
  if (input.text().length() < min_query_length) {
    return;
  }

  // Don't issue queries for input likely to be a URL.
  if (IsInputLikelyURL(input)) {
    return;
  }

  // We currently only provide asynchronous matches.
  if (!input.want_asynchronous_matches()) {
    return;
  }

  Stop(true, false);

  input_ = input;

  // Create a request for suggestions, routing completion to
  base::BindOnce(&DocumentProvider::OnDocumentSuggestionsLoaderAvailable,
                 weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DocumentProvider::OnURLLoadComplete,
                     base::Unretained(this) /* own SimpleURLLoader */);

  done_ = false;  // Set true in callbacks.
  client_->GetDocumentSuggestionsService(/*create_if_necessary=*/true)
      ->CreateDocumentSuggestionsRequest(
          input.text(), client_->GetTemplateURLService(),
          base::BindOnce(
              &DocumentProvider::OnDocumentSuggestionsLoaderAvailable,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &DocumentProvider::OnURLLoadComplete,
              base::Unretained(this) /* this owns SimpleURLLoader */));
}

void DocumentProvider::Stop(bool clear_cached_results,
                            bool due_to_user_inactivity) {
  TRACE_EVENT0("omnibox", "DocumentProvider::Stop");
  if (loader_)
    LogOmniboxDocumentRequest(DOCUMENT_REQUEST_INVALIDATED);
  loader_.reset();
  auto* document_suggestions_service =
      client_->GetDocumentSuggestionsService(/*create_if_necessary=*/false);
  if (document_suggestions_service != nullptr) {
    document_suggestions_service->StopCreatingDocumentSuggestionsRequest();
  }

  done_ = true;

  if (clear_cached_results) {
    matches_.clear();
  }
}

void DocumentProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Not supported by this provider.
  return;
}

void DocumentProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(metrics::OmniboxEventProto::DOCUMENT);
  new_entry.set_provider_done(done_);

  if (field_trial_triggered_ || field_trial_triggered_in_session_) {
    std::vector<uint32_t> field_trial_hashes;
    OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
    for (uint32_t trial : field_trial_hashes) {
      if (field_trial_triggered_) {
        new_entry.mutable_field_trial_triggered()->Add(trial);
      }
      if (field_trial_triggered_in_session_) {
        new_entry.mutable_field_trial_triggered_in_session()->Add(trial);
      }
    }
  }
}

void DocumentProvider::ResetSession() {
  field_trial_triggered_in_session_ = false;
  field_trial_triggered_ = false;
}

DocumentProvider::DocumentProvider(AutocompleteProviderClient* client,
                                   AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_DOCUMENT),
      field_trial_triggered_(false),
      field_trial_triggered_in_session_(false),
      backoff_for_session_(false),
      client_(client),
      listener_(listener),
      weak_ptr_factory_(this) {}

DocumentProvider::~DocumentProvider() {}

void DocumentProvider::OnURLLoadComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK(!done_);
  DCHECK_EQ(loader_.get(), source);

  LogOmniboxDocumentRequest(DOCUMENT_REPLY_RECEIVED);

  const bool results_updated =
      response_body && source->NetError() == net::OK &&
      (source->ResponseInfo() && source->ResponseInfo()->headers &&
       source->ResponseInfo()->headers->response_code() == 200) &&
      UpdateResults(SearchSuggestionParser::ExtractJsonData(
          source, std::move(response_body)));
  loader_.reset();
  done_ = true;
  listener_->OnProviderUpdate(results_updated);
}

bool DocumentProvider::UpdateResults(const std::string& json_data) {
  base::Optional<base::Value> response =
      base::JSONReader::Read(json_data, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!response)
    return false;

  return ParseDocumentSearchResults(*response, &matches_);
}

void DocumentProvider::OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  loader_ = std::move(loader);
  LogOmniboxDocumentRequest(DOCUMENT_REQUEST_SENT);
}

// static
base::string16 DocumentProvider::GenerateLastModifiedString(
    const std::string& modified_timestamp_string,
    base::Time now) {
  if (modified_timestamp_string.empty())
    return base::string16();
  base::Time modified_time;
  if (!base::Time::FromString(modified_timestamp_string.c_str(),
                              &modified_time))
    return base::string16();

  // Use shorthand if the times fall on the same day or in the same year.
  base::Time::Exploded exploded_modified_time;
  base::Time::Exploded exploded_now;
  modified_time.LocalExplode(&exploded_modified_time);
  now.LocalExplode(&exploded_now);
  if (exploded_modified_time.year == exploded_now.year) {
    if (exploded_modified_time.month == exploded_now.month &&
        exploded_modified_time.day_of_month == exploded_now.day_of_month) {
      // Same local calendar day - use localized time.
      return base::TimeFormatTimeOfDay(modified_time);
    }
    // Same year but not the same day: use abbreviated month/day ("Jan 1").
    return base::TimeFormatWithPattern(modified_time, "MMMd");
  }

  // No shorthand; display full MM/DD/YYYY.
  return base::TimeFormatShortDateNumeric(modified_time);
}

// static
base::string16 GetProductDescriptionString(const std::string& mimetype) {
  if (mimetype == kDocumentMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_DOCUMENT);
  if (mimetype == kFormMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_FORM);
  if (mimetype == kSpreadsheetMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_SPREADSHEET);
  if (mimetype == kPresentationMimetype)
    return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_PRESENTATION);
  // Fallback to "Drive" for other filetypes.
  return l10n_util::GetStringUTF16(IDS_DRIVE_SUGGESTION_GENERAL);
}

bool DocumentProvider::ParseDocumentSearchResults(const base::Value& root_val,
                                                  ACMatches* matches) {
  const base::DictionaryValue* root_dict = nullptr;
  const base::ListValue* results_list = nullptr;
  if (!root_val.GetAsDictionary(&root_dict)) {
    return false;
  }

  // The server may ask the client to back off, in which case we back off for
  // the session.
  // TODO(skare): Respect retryDelay if provided, ideally by calling via gRPC.
  if (ResponseContainsBackoffSignal(root_dict)) {
    backoff_for_session_ = true;
    return false;
  }

  // Otherwise parse the results.
  if (!root_dict->GetList("results", &results_list)) {
    return false;
  }
  size_t num_results = results_list->GetSize();
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.DocumentSuggest.ResultCount", num_results);

  // Create a synthetic score, for when there's no signal from the API.
  // For now, allow setting of each of three scores from Finch.
  int score0 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult1", 1100);
  int score1 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult2", 700);
  int score2 = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDocumentProvider, "DocumentScoreResult3", 300);
  // During development/quality iteration we may wish to defeat server scores.
  bool use_server_scores = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentUseServerScore", true);

  // Some users may be in a counterfactual study arm in which we perform all
  // necessary work but do not forward the autocomplete matches.
  bool in_counterfactual_group = base::GetFieldTrialParamByFeatureAsBool(
      omnibox::kDocumentProvider, "DocumentProviderCounterfactualArm", false);

  // Clear the previous results now that new results are available.
  matches->clear();
  // Ensure server's suggestions are added with monotonically decreasing scores.
  // When previous_score is >= 0, it enforces a maximum score for subsequent
  // results.
  int previous_score = -1;
  for (size_t i = 0; i < num_results; i++) {
    if (matches->size() >= provider_max_matches_) {
      break;
    }
    const base::DictionaryValue* result = nullptr;
    if (!results_list->GetDictionary(i, &result)) {
      return false;
    }
    base::string16 title;
    base::string16 url;
    result->GetString("title", &title);
    result->GetString("url", &url);
    if (title.empty() || url.empty()) {
      continue;
    }
    int relevance = 0;
    switch (matches->size()) {
      case 0:
        relevance = score0;
        break;
      case 1:
        relevance = score1;
        break;
      case 2:
        relevance = score2;
        break;
      default:
        break;
    }
    int server_score;
    if (use_server_scores && result->GetInteger("score", &server_score)) {
      if (previous_score >= 0 && server_score >= previous_score) {
        server_score = previous_score - 1;
      }
      relevance = server_score;
      previous_score = relevance;
    }
    relevance = std::max(relevance, 0);
    AutocompleteMatch match(this, relevance, false,
                            AutocompleteMatchType::DOCUMENT_SUGGESTION);
    // Use full URL for displayed text and navigation. Use "originalUrl" for
    // deduping if present.
    match.fill_into_edit = url;
    match.destination_url = GURL(url);
    base::string16 original_url;
    std::string mimetype;
    if (result->GetString("originalUrl", &original_url)) {
      GURL stripped_url = GURL(original_url);
      if (base::FeatureList::IsEnabled(omnibox::kDedupeGoogleDriveURLs))
        stripped_url = GetURLForDeduping(stripped_url);
      if (stripped_url.is_valid())
        match.stripped_destination_url = stripped_url;
    }
    match.contents = AutocompleteMatch::SanitizeString(title);
    match.contents_class = Classify(match.contents, input_.text());
    const base::DictionaryValue* metadata = nullptr;
    if (result->GetDictionary("metadata", &metadata)) {
      if (metadata->GetString("mimeType", &mimetype)) {
        match.document_type = GetIconForMIMEType(mimetype);
      }
      std::string update_time;
      metadata->GetString("updateTime", &update_time);
      if (!update_time.empty()) {
        match.description = l10n_util::GetStringFUTF16(
            IDS_DRIVE_SUGGESTION_DESCRIPTION_TEMPLATE,
            GenerateLastModifiedString(update_time, base::Time::Now()),
            GetProductDescriptionString(mimetype));
      } else {
        match.description = GetProductDescriptionString(mimetype);
      }
      AutocompleteMatch::AddLastClassificationIfNecessary(
          &match.description_class, 0, ACMatchClassification::DIM);
    }
    match.transition = ui::PAGE_TRANSITION_GENERATED;
    if (!in_counterfactual_group) {
      matches->push_back(match);
    }
    field_trial_triggered_ = true;
    field_trial_triggered_in_session_ = true;
  }
  return true;
}

// static
ACMatchClassifications DocumentProvider::Classify(
    const base::string16& text,
    const base::string16& input_text) {
  TermMatches term_matches = FindTermMatches(input_text, text);
  return ClassifyTermMatches(term_matches, text.size(),
                             ACMatchClassification::MATCH,
                             ACMatchClassification::NONE);
}

// static
const GURL DocumentProvider::GetURLForDeduping(const GURL& url) {
  // We aim to prevent duplicate Drive URLs to appear between the Drive document
  // search provider and history/bookmark entries.
  // Drive URLs take on two core forms, and may have request parameters.
  // Additionally, we may have redirector URLs which wrap a drive URL.
  // All URLs are canonicalized to a GURL form only used for deduplication and
  // not guaranteed to be usable for navigation.
  // URLs of the following forms are handled:
  // https://drive.google.com/[a/domain.tld]/open?id=(id)
  // https://docs.google.com/[a/domain.tld/]document/d/(id)/[...]
  // https://docs.google.com/[a/domain.tld/]spreadsheets/d/(id)/edit#gid=12345
  // https://docs.google.com/[a/domain.tld/]presentation/d/(id)/edit#slide=id.g12345a_0_26
  // https://www.google.com/url?[...]url=https://drive.google.com/a/domain.tld/open?id%3D(id)[%26D...][&...]
  // where id is comprised of characters in [0-9A-Za-z\-_] = [\w\-]
  std::string id;

  if (url.host() == "drive.google.com") {
    static re2::LazyRE2 path_regex = {"^/(?:a/[\\w\\.]+/)?open$"};
    if (RE2::PartialMatch(url.path(), *path_regex))
      net::GetValueForKeyInQuery(url, "id", &id);
  } else if (url.host() == "docs.google.com") {
    static re2::LazyRE2 doc_link_regex = {
        "^/(?:a/[\\w\\.]+/)?(?:document|spreadsheets|presentation|forms)/d/"
        "([\\w-]+)/"};
    RE2::PartialMatch(url.path(), *doc_link_regex, &id);
  } else if (url.host() == "www.google.com" && url.path() == "/url") {
    // Redirect links wrapping a drive.google.com/open?id= link.
    static re2::LazyRE2 redirect_link_regex = {
        "^[^#]*url=https://drive\\.google\\.com/(?:a/[\\w\\.]+/"
        ")?open\\?id%3D(.*?)(?:%26|#|&|$)"};
    RE2::PartialMatch(url.query(), *redirect_link_regex, &id);
  }

  if (id.empty()) {
    return GURL();
  } else {
    // Canonicalize to the /open form without any extra args.
    // This is similar to what we expect from the server.
    return GURL("https://drive.google.com/open?id=" + id);
  }
}
