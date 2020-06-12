// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"

class TokenizedString;

namespace local_search_service {

// A search backend that linearly scans all documents in the storage and finds
// documents that match the input query. Search is done by matching query with
// documents' search tags.
class LinearMapSearch {
 public:
  LinearMapSearch();
  ~LinearMapSearch();

  LinearMapSearch(const LinearMapSearch&) = delete;
  LinearMapSearch& operator=(const LinearMapSearch&) = delete;

  // Returns number of data items.
  uint64_t GetSize();

  // Adds or updates data.
  // IDs of data should not be empty.
  void AddOrUpdate(const std::vector<Data>& data);

  // Deletes data with |ids| and returns number of items deleted.
  // If an id doesn't exist in the LinearMapSearch, no operation will be done.
  // IDs should not be empty.
  uint32_t Delete(const std::vector<std::string>& ids);

  // Returns matching results for a given query.
  // Zero |max_results| means no max.
  // For each data in the index, we return the 1st search tag that matches
  // the query (i.e. above the threshold). Client should put the most
  // important search tag first when registering the data in the index.
  ResponseStatus Find(const base::string16& query,
                      uint32_t max_results,
                      std::vector<Result>* results);

  void SetSearchParams(const SearchParams& search_params);

  SearchParams GetSearchParams();

 private:
  // Returns all search results for a given query.
  std::vector<Result> GetSearchResults(const base::string16& query,
                                       uint32_t max_results) const;

  // A map from key to a vector of (tag-id, tokenized tag).
  std::map<
      std::string,
      std::vector<std::pair<std::string, std::unique_ptr<TokenizedString>>>>
      data_;

  // Search parameters.
  SearchParams search_params_;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_
