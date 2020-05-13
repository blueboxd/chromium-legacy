// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <unordered_map>
#include <vector>

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

namespace local_search_service {
class Index;
class LocalSearchService;
}  // namespace local_search_service

namespace chromeos {
namespace settings {

struct SearchConcept;

// Processes all registered search tags by adding/removing them from
// LocalSearchService and providing metadata via GetCanonicalTagMetadata().
class SearchTagRegistry {
 public:
  SearchTagRegistry(
      local_search_service::LocalSearchService* local_search_service);
  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;
  virtual ~SearchTagRegistry();

  void AddSearchTags(const std::vector<SearchConcept>& search_tags);
  void RemoveSearchTags(const std::vector<SearchConcept>& search_tags);

  // Returns the tag metadata associated with |canonical_message_id|, which must
  // be one of the canonical IDS_OS_SETTINGS_TAG_* identifiers used for a search
  // tag. If no metadata is available or if |canonical_message_id| instead
  // refers to an alternate tag's ID, null is returned.
  const SearchConcept* GetCanonicalTagMetadata(int canonical_message_id) const;

 private:
  local_search_service::Index* index_;
  std::unordered_map<int, const SearchConcept*> canonical_id_to_metadata_map_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_
