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

class TokenizedString;

namespace local_search_service {

struct Data {
  // Identifier of the data item, should be unique across the registry. Clients
  // will decide what ids to use, they could be paths, urls or any opaque string
  // identifiers.
  // Ideally IDs should persist across sessions, but this is not strictly
  // required now because data is not persisted across sessions.
  std::string id;

  // Data item will be matched between its search tags and query term.
  std::vector<base::string16> search_tags;

  Data(const std::string& id, const std::vector<base::string16>& search_tags);
  Data();
  Data(const Data& data);
  ~Data();
};

struct SearchParams {
  double relevance_threshold = 0.3;
  double partial_match_penalty_rate = 0.9;
  bool use_prefix_only = false;
  bool use_weighted_ratio = true;
  bool use_edit_distance = false;
};

// A numeric range used to represent the start and end position.
struct Range {
  uint32_t start;
  uint32_t end;
};

// Result is one item that matches a given query. It contains the id of the item
// and its matching score.
struct Result {
  // Id of the data.
  std::string id;
  // Relevance score, in the range of [0,1].
  double score;
  // Matching ranges.
  std::vector<Range> hits;

  Result();
  Result(const Result& result);
  ~Result();
};

// Status of the search attempt.
// More will be added later.
enum class ResponseStatus {
  kUnknownError = 0,
  // Query is empty.
  kEmptyQuery = 1,
  // Index is empty (i.e. no data).
  kEmptyIndex = 2,
  // Search operation is successful. But there could be no matching item and
  // result list is empty.
  kSuccess = 3
};

// A local search service Index.
// It has a registry of searchable data, which can be updated. It also runs a
// synchronous search function to find matching items for a given query.
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
  void AddOrUpdate(const std::vector<local_search_service::Data>& data);

  // Deletes data with |ids| and returns number of items deleted.
  // If an id doesn't exist in the Index, no operation will be done.
  // IDs should not be empty.
  uint32_t Delete(const std::vector<std::string>& ids);

  // Returns matching results for a given query.
  // Zero |max_results| means no max.
  local_search_service::ResponseStatus Find(
      const base::string16& query,
      uint32_t max_results,
      std::vector<local_search_service::Result>* results);

  void SetSearchParams(const local_search_service::SearchParams& search_params);

  void GetSearchParamsForTesting(double* relevance_threshold,
                                 double* partial_match_penalty_rate,
                                 bool* use_prefix_only,
                                 bool* use_weighted_ratio,
                                 bool* use_edit_distance);

 private:
  // Returns all search results for a given query.
  std::vector<local_search_service::Result> GetSearchResults(
      const base::string16& query,
      uint32_t max_results) const;

  // A map from key to tokenized search-tags.
  std::map<std::string, std::vector<std::unique_ptr<TokenizedString>>> data_;

  // Search parameters.
  local_search_service::SearchParams search_params_;

  base::WeakPtrFactory<Index> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
