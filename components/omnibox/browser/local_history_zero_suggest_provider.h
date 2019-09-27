// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;

namespace history {
class HistoryDBTask;
class QueryResults;
}  // namespace history

// Autocomplete provider for on-focus zero-prefix query suggestions from local
// history when Google is the default search engine.
class LocalHistoryZeroSuggestProvider : public AutocompleteProvider {
 public:
  // Creates and returns an instance of this provider.
  static LocalHistoryZeroSuggestProvider* Create(
      AutocompleteProviderClient* client,
      AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

 private:
  LocalHistoryZeroSuggestProvider(AutocompleteProviderClient* client,
                                  AutocompleteProviderListener* listener);
  ~LocalHistoryZeroSuggestProvider() override;

  // Creates autocomplete matches from |url_matches| and notifies |listener_|.
  void OnQueryURLDatabaseComplete(const AutocompleteInput& input,
                                  const history::URLRows& url_matches);

  // Called when the query results from HistoryService::QueryHistory are ready.
  // Deletes URLs in |results| that would generate |suggestion|.
  void OnHistoryQueryResults(const base::string16& suggestion,
                             history::QueryResults results);

  // The maximum number of matches to return.
  const size_t max_matches_;

  // Used to filter out deleted query suggestions in order to prevent them from
  // reappearing before the corresponding URLs are asynchronously deleted.
  std::set<base::string16> deleted_suggestions_set_;

  // Client for accessing TemplateUrlService, prefs, etc.
  AutocompleteProviderClient* const client_;

  // Listener to notify when matches are available.
  AutocompleteProviderListener* const listener_;

  // Used for the async tasks querying the HistoryService.
  base::CancelableTaskTracker history_task_tracker_;

  // Task ID for the history::HistoryDBTask running on history backend thread.
  base::CancelableTaskTracker::TaskId history_db_task_id_;

  base::WeakPtrFactory<LocalHistoryZeroSuggestProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalHistoryZeroSuggestProvider);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_LOCAL_HISTORY_ZERO_SUGGEST_PROVIDER_H_
