// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_HANDLER_H_

#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/services/local_search_service/index_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace local_search_service {
class LocalSearchServiceImpl;
}  // namespace local_search_service

namespace chromeos {
namespace settings {

class OsSettingsLocalizedStringsProvider;

// Handles search queries for Chrome OS settings. Search() is expected to be
// invoked by the settings UI as well as the the Launcher search UI. Search
// results are obtained by matching the provided query against search tags
// indexed in the LocalSearchService and cross-referencing results with
// OsSettingsLocalizedStringsProvider.
//
// SearchHandler returns at most |kNumMaxResults| results; searches which do not
// provide any matches result in an empty results array.
class SearchHandler : public mojom::SearchHandler, public KeyedService {
 public:
  // Maximum number of results returned by a Search() call.
  static const size_t kNumMaxResults;

  SearchHandler(
      OsSettingsLocalizedStringsProvider* strings_provider,
      local_search_service::LocalSearchServiceImpl* local_search_service);
  ~SearchHandler() override;

  SearchHandler(const SearchHandler& other) = delete;
  SearchHandler& operator=(const SearchHandler& other) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::SearchHandler> pending_receiver);

  // Synchronous search implementation (for in-process clients).
  std::vector<mojom::SearchResultPtr> Search(const base::string16& query);

  // mojom::SearchHandler:
  void Search(const base::string16& query, SearchCallback callback) override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::vector<mojom::SearchResultPtr> GenerateSearchResultsArray(
      const std::vector<local_search_service::Result>&
          local_search_service_results);
  mojom::SearchResultPtr ResultToSearchResult(
      const local_search_service::Result& result);

  OsSettingsLocalizedStringsProvider* strings_provider_;
  local_search_service::IndexImpl* index_;

  // Note: Expected to have multiple clients, so a ReceiverSet is used.
  mojo::ReceiverSet<mojom::SearchHandler> receivers_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_HANDLER_H_
