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

std::vector<local_search_service::Data> ConceptVectorToDataVector(
    const std::vector<SearchConcept>& search_tags) {
  std::vector<local_search_service::Data> data_list;

  for (const auto& concept : search_tags) {
    std::vector<base::string16> search_tags;

    // Add the canonical tag.
    search_tags.push_back(
        l10n_util::GetStringUTF16(concept.canonical_message_id));

    // Add all alternate tags.
    for (size_t i = 0; i < SearchConcept::kMaxAltTagsPerConcept; ++i) {
      int curr_alt_tag = concept.alt_tag_ids[i];
      if (curr_alt_tag == SearchConcept::kAltTagEnd)
        break;
      search_tags.push_back(l10n_util::GetStringUTF16(curr_alt_tag));
    }

    // Note: A stringified version of the canonical tag message ID is used as
    // the identifier for this search data.
    data_list.emplace_back(base::NumberToString(concept.canonical_message_id),
                           search_tags);
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
  for (const auto& concept : search_tags)
    canonical_id_to_metadata_map_[concept.canonical_message_id] = &concept;
}

void SearchTagRegistry::RemoveSearchTags(
    const std::vector<SearchConcept>& search_tags) {
  if (!base::FeatureList::IsEnabled(features::kNewOsSettingsSearch))
    return;

  std::vector<std::string> ids;
  for (const auto& concept : search_tags) {
    canonical_id_to_metadata_map_.erase(concept.canonical_message_id);
    ids.push_back(base::NumberToString(concept.canonical_message_id));
  }

  index_->Delete(ids);
}

const SearchConcept* SearchTagRegistry::GetCanonicalTagMetadata(
    int canonical_message_id) const {
  const auto it = canonical_id_to_metadata_map_.find(canonical_message_id);
  if (it == canonical_id_to_metadata_map_.end())
    return nullptr;
  return it->second;
}

}  // namespace settings
}  // namespace chromeos
