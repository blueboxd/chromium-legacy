// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/local_search_service/index.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_concept.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

std::vector<int> GetMessageIds(const SearchConcept& concept) {
  // Start with only the canonical ID.
  std::vector<int> alt_tag_message_ids{concept.canonical_message_id};

  // Add alternate IDs, if they exist.
  for (size_t i = 0; i < SearchConcept::kMaxAltTagsPerConcept; ++i) {
    int curr_alt_tag_message_id = concept.alt_tag_ids[i];
    if (curr_alt_tag_message_id == SearchConcept::kAltTagEnd)
      break;
    alt_tag_message_ids.push_back(curr_alt_tag_message_id);
  }

  return alt_tag_message_ids;
}

std::vector<local_search_service::Data> ConceptVectorToDataVector(
    const std::vector<SearchConcept>& search_tags) {
  std::vector<local_search_service::Data> data_list;

  for (const auto& concept : search_tags) {
    // Create a list of Content objects, which use the stringified version of
    // message IDs as identifiers.
    std::vector<local_search_service::Content> content_list;
    for (int message_id : GetMessageIds(concept)) {
      content_list.emplace_back(
          /*id=*/base::NumberToString(message_id),
          /*content=*/l10n_util::GetStringUTF16(message_id));
    }

    // Use the stringified version of the canonical message ID as an identifier.
    data_list.emplace_back(base::NumberToString(concept.canonical_message_id),
                           content_list);
  }

  return data_list;
}

}  // namespace

SearchTagRegistry::SearchTagRegistry(
    local_search_service::LocalSearchService* local_search_service)
    : index_(local_search_service->GetIndex(
          local_search_service::IndexId::kCrosSettings)) {}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::AddSearchTags(
    const std::vector<SearchConcept>& search_tags) {
  if (!base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
    return;

  index_->AddOrUpdate(ConceptVectorToDataVector(search_tags));

  // Add each concept to the map. Note that it is safe to take the address of
  // each concept because all concepts are allocated via static
  // base::NoDestructor objects in the Get*SearchConcepts() helper functions.
  for (const auto& concept : search_tags) {
    for (int message_id : GetMessageIds(concept))
      message_id_to_metadata_map_[message_id] = &concept;
  }
}

void SearchTagRegistry::RemoveSearchTags(
    const std::vector<SearchConcept>& search_tags) {
  if (!base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
    return;

  std::vector<std::string> ids;
  for (const auto& concept : search_tags) {
    for (int message_id : GetMessageIds(concept))
      message_id_to_metadata_map_.erase(message_id);
    ids.push_back(base::NumberToString(concept.canonical_message_id));
  }

  index_->Delete(ids);
}

const SearchConcept* SearchTagRegistry::GetTagMetadata(
    int canonical_message_id) const {
  const auto it = message_id_to_metadata_map_.find(canonical_message_id);
  if (it == message_id_to_metadata_map_.end())
    return nullptr;
  return it->second;
}

}  // namespace settings
}  // namespace chromeos
