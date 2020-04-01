// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_result_icon.mojom.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

const int32_t kLocalSearchServiceMaxResults = 10;

}  // namespace

// static
const size_t SearchHandler::kNumMaxResults = 5;

SearchHandler::SearchHandler(
    OsSettingsLocalizedStringsProvider* strings_provider,
    local_search_service::LocalSearchServiceImpl* local_search_service)
    : strings_provider_(strings_provider),
      index_(local_search_service->GetIndexImpl(
          local_search_service::IndexId::kCrosSettings)) {}

SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

std::vector<mojom::SearchResultPtr> SearchHandler::Search(
    const base::string16& query) {
  std::vector<local_search_service::Result> local_search_service_results;
  local_search_service::ResponseStatus response_status = index_->Find(
      query, kLocalSearchServiceMaxResults, &local_search_service_results);

  if (response_status != local_search_service::ResponseStatus::kSuccess) {
    LOG(ERROR) << "Cannot search; LocalSearchService returned "
               << static_cast<int>(response_status)
               << ". Returning empty results array.";
    return {};
  }

  return GenerateSearchResultsArray(local_search_service_results);
}

void SearchHandler::Search(const base::string16& query,
                           SearchCallback callback) {
  std::move(callback).Run(Search(query));
}

std::vector<mojom::SearchResultPtr> SearchHandler::GenerateSearchResultsArray(
    const std::vector<local_search_service::Result>&
        local_search_service_results) {
  std::vector<mojom::SearchResultPtr> search_results;
  for (const auto& result : local_search_service_results) {
    mojom::SearchResultPtr result_ptr = ResultToSearchResult(result);
    if (result_ptr)
      search_results.push_back(std::move(result_ptr));

    // Limit the number of results to return.
    if (search_results.size() == kNumMaxResults)
      break;
  }

  return search_results;
}

mojom::SearchResultPtr SearchHandler::ResultToSearchResult(
    const local_search_service::Result& result) {
  int message_id;

  // The result's ID is expected to be a stringified int.
  if (!base::StringToInt(result.id, &message_id))
    return nullptr;

  const SearchConcept* concept =
      strings_provider_->GetCanonicalTagMetadata(message_id);

  // If the concept was not registered, no metadata is available. This can occur
  // if the search tag was dynamically unregistered during the asynchronous
  // Find() call.
  if (!concept)
    return nullptr;

  return mojom::SearchResult::New(l10n_util::GetStringUTF16(message_id),
                                  concept->url_path_with_parameters,
                                  concept->icon);
}

void SearchHandler::Shutdown() {
  strings_provider_ = nullptr;
  index_ = nullptr;
  receivers_.Clear();
}

}  // namespace settings
}  // namespace chromeos
