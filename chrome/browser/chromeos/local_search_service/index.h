// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"

namespace local_search_service {

class LinearMapSearch;

// A local search service Index.
// It is the client-facing API for search and indexing. It owns different
// backends that provide actual data storage/indexing/search functions.
class Index {
 public:
  Index();
  ~Index();

  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;

  // Returns number of data items.
  uint64_t GetSize();

  // Adds or updates data.
  // IDs of data should not be empty.
  void AddOrUpdate(const std::vector<Data>& data);

  // Deletes data with |ids| and returns number of items deleted.
  // If an id doesn't exist in the Index, no operation will be done.
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

  SearchParams GetSearchParamsForTesting();
 private:
  // TODO(jiameng): Currently linear map is the only backend supported. We will
  // add inverted index in the next CLs.
  std::unique_ptr<LinearMapSearch> linear_map_search_;
  base::WeakPtrFactory<Index> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
