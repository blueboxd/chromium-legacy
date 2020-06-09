// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_

#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/local_search_service/index.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

namespace local_search_service {
class Index;
class LocalSearchService;
}  // namespace local_search_service

namespace chromeos {
namespace settings {

struct SearchConcept;

// Processes all registered search tags by adding/removing them from
// LocalSearchService and providing metadata via GetTagMetadata().
class SearchTagRegistry {
 public:
  SearchTagRegistry(
      local_search_service::LocalSearchService* local_search_service);
  SearchTagRegistry(const SearchTagRegistry& other) = delete;
  SearchTagRegistry& operator=(const SearchTagRegistry& other) = delete;
  virtual ~SearchTagRegistry();

  void AddSearchTags(const std::vector<SearchConcept>& search_tags);
  void RemoveSearchTags(const std::vector<SearchConcept>& search_tags);

  // Returns the tag metadata associated with |result_id|, which is the ID
  // returned by the LocalSearchService. If no metadata is available, null is
  // returned.
  const SearchConcept* GetTagMetadata(const std::string& result_id) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchTagRegistryTest, AddAndRemove);

  static std::string ToResultId(const SearchConcept& concept);

  std::vector<local_search_service::Data> ConceptVectorToDataVector(
      const std::vector<SearchConcept>& search_tags);

  // Index used by the LocalSearchService for string matching.
  local_search_service::Index* index_;

  // In-memory cache of all results which have been added to the
  // LocalSearchService. Contents are kept in sync with |index_|.
  std::unordered_map<std::string, const SearchConcept*>
      result_id_to_metadata_list_map_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SEARCH_SEARCH_TAG_REGISTRY_H_
