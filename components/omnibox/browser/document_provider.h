// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the document autocomplete provider. This experimental
// provider uses an experimental API with keys and endpoint provided at
// developer build-time, so it is feature-flagged off by default.

#ifndef COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteProviderListener;
class AutocompleteProviderClient;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Autocomplete provider for personalized documents owned or readable by the
// signed-in user. In practice this is a second request in parallel with that
// to the default search provider.
class DocumentProvider : public AutocompleteProvider {
 public:
  // Creates and returns an instance of this provider.
  static DocumentProvider* Create(AutocompleteProviderClient* client,
                                  AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;
  void ResetSession() override;

  // Registers a client-side preference to enable document suggestions.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Builds a GURL to use for deduping against other document/history
  // suggestions. Multiple URLs may refer to the same document.
  // Returns an empty GURL if not a recognized Docs URL.
  // The URL returned is not guaranteed to be navigable and should only be used
  // as a deduping token.
  static const GURL GetURLForDeduping(const GURL& url);

 private:
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, CheckFeatureBehindFlag);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteNoIncognito);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteNoSync);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteClientSettingOff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           CheckFeaturePrerequisiteServerBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, IsInputLikelyURL);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, ParseDocumentSearchResults);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ProductDescriptionStringsAndAccessibleLabels);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTies);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTiesCascade);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsBreakTiesZeroLimit);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithBackoff);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest,
                           ParseDocumentSearchResultsWithIneligibleFlag);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, GenerateLastModifiedString);
  FRIEND_TEST_ALL_PREFIXES(DocumentProviderTest, Scoring);

  DocumentProvider(AutocompleteProviderClient* client,
                   AutocompleteProviderListener* listener);

  ~DocumentProvider() override;

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsDocumentProviderAllowed(AutocompleteProviderClient* client);

  // Determines if the input is a URL, or is the start of the user entering one.
  // We avoid queries for these cases for quality and scaling reasons.
  static bool IsInputLikelyURL(const AutocompleteInput& input);

  // Called when loading is complete.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  // The function updates |matches_| with data parsed from |json_data|.
  // The update is not performed if |json_data| is invalid.
  // Returns whether |matches_| changed.
  bool UpdateResults(const std::string& json_data);

  // Callback for when the loader is available with a valid token.
  void OnDocumentSuggestionsLoaderAvailable(
      std::unique_ptr<network::SimpleURLLoader> loader);

  // Parses document search result JSON.
  // Returns true if |matches| was populated with fresh suggestions.
  bool ParseDocumentSearchResults(const base::Value& root_val,
                                  ACMatches* matches);

  // Generates the localized last-modified timestamp to present to the user.
  // Full date for old files, mm/dd within the same calendar year, or time-of-
  // day if a file was modified on the same date.
  // |now| should generally be base::Time::Now() but is passed in for testing.
  static base::string16 GenerateLastModifiedString(
      const std::string& modified_timestamp_string,
      base::Time now);

  // Returns a set of classifications that highlight all the occurrences of
  // |input_text| at word breaks in |text|. E.g., given |input_text|
  // "rain if you dare" and |text| "how to tell if your kitten is a rainbow",
  // will return the classifications:
  //             __ ___              ____
  // how to tell if your kitten is a rainbow
  // ^           ^ ^^   ^            ^  ^
  // NONE        M |M   |            |  NONE
  //               NONE NONE         MATCH
  static ACMatchClassifications Classify(const base::string16& input_text,
                                         const base::string16& text);

  // Whether a field trial has triggered for this query and this session,
  // respectively. Works similarly to BaseSearchProvider, though this class does
  // not inherit from it.
  bool field_trial_triggered_;
  bool field_trial_triggered_in_session_;

  // Whether the server has instructed us to backoff for this session (in
  // cases where the corpus is uninteresting).
  bool backoff_for_session_;

  // Client for accessing TemplateUrlService, prefs, etc.
  AutocompleteProviderClient* client_;

  // Listener to notify when results are available.
  AutocompleteProviderListener* listener_;

  // Saved when starting a new autocomplete request so that it can be retrieved
  // when responses return asynchronously.
  AutocompleteInput input_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // For callbacks that may be run after destruction. Must be declared last.
  base::WeakPtrFactory<DocumentProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DocumentProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_DOCUMENT_PROVIDER_H_
