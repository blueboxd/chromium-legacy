// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_result_icon.mojom.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

const int32_t kLocalSearchServiceMaxLatencyMs = 3000;
const int32_t kLocalSearchServiceMaxResults = 10;

}  // namespace

// static
const size_t SearchHandler::kNumMaxResults = 5;

SearchHandler::SearchHandler(
    OsSettingsLocalizedStringsProvider* strings_provider,
    local_search_service::mojom::LocalSearchService* local_search_service)
    : strings_provider_(strings_provider) {
  DCHECK(strings_provider_);
  local_search_service->GetIndex(
      local_search_service::mojom::LocalSearchService::IndexId::CROS_SETTINGS,
      index_remote_.BindNewPipeAndPassReceiver());
}

SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const base::string16& query,
                           SearchCallback callback) {
  index_remote_->Find(
      query, kLocalSearchServiceMaxLatencyMs, kLocalSearchServiceMaxResults,
      base::BindOnce(&SearchHandler::OnLocalSearchServiceResults,
                     base::Unretained(this), std::move(callback)));
}

void SearchHandler::OnLocalSearchServiceResults(
    SearchCallback callback,
    local_search_service::mojom::ResponseStatus response_status,
    base::Optional<std::vector<local_search_service::mojom::ResultPtr>>
        results) {
  switch (response_status) {
    case local_search_service::mojom::ResponseStatus::UNKNOWN_ERROR:
    case local_search_service::mojom::ResponseStatus::EMPTY_QUERY:
    case local_search_service::mojom::ResponseStatus::EMPTY_INDEX:
      LOG(ERROR) << "Cannot search; LocalSearchService returned "
                 << response_status << ". Returning empty results array.";
      std::move(callback).Run({});
      return;
    case local_search_service::mojom::ResponseStatus::SUCCESS:
      DCHECK(results);
      ReturnSuccessfulResults(std::move(callback), std::move(results));
      return;
  }

  NOTREACHED() << "Invalid response status: " << response_status << ".";
}

void SearchHandler::ReturnSuccessfulResults(
    SearchCallback callback,
    base::Optional<std::vector<local_search_service::mojom::ResultPtr>>
        results) {
  std::vector<mojom::SearchResultPtr> search_results;
  for (const auto& result : *results) {
    mojom::SearchResultPtr result_ptr = ResultToSearchResult(*result);
    if (result_ptr)
      search_results.push_back(std::move(result_ptr));

    // Limit the number of results to return.
    if (search_results.size() == kNumMaxResults)
      break;
  }

  std::move(callback).Run(std::move(search_results));
}

mojom::SearchResultPtr SearchHandler::ResultToSearchResult(
    const local_search_service::mojom::Result& result) {
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
  receivers_.Clear();
  index_remote_.reset();
}

}  // namespace settings
}  // namespace chromeos
