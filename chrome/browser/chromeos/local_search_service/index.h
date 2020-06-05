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

struct Content {
  // An identifier for the content in Data.
  std::string id;
  base::string16 content;
  Content(const std::string& id, const base::string16& content);
  Content();
  Content(const Content& content);
  ~Content();
};

struct Data {
  // Identifier of the data item, should be unique across the registry. Clients
  // will decide what ids to use, they could be paths, urls or any opaque string
  // identifiers.
  // Ideally IDs should persist across sessions, but this is not strictly
  // required now because data is not persisted across sessions.
  std::string id;

  // Data item will be matched between its search tags and query term.
  std::vector<Content> contents;

  Data(const std::string& id, const std::vector<Content>& contents);
  Data();
  Data(const Data& data);
  ~Data();
};

struct SearchParams {
  double relevance_threshold = 0.32;
  double partial_match_penalty_rate = 0.9;
  bool use_prefix_only = false;
  bool use_edit_distance = false;
};

struct Position {
  std::string content_id;
  // TODO(jiameng): |start| and |end| will be implemented for inverted index
  // later.
  uint32_t start;
  uint32_t length;
};

// Result is one item that matches a given query. It contains the id of the item
// and its matching score.
struct Result {
  // Id of the data.
  std::string id;
  // Relevance score.
  // Currently only linear map is implemented with fuzzy matching and score will
  // always be in [0,1]. In the future, when an inverted index is implemented,
  // the score will not be in this range any more. Client will be able to select
  // a search backend to use (linear map vs inverted index) and hence client
  // will be able to expect the range of the scores.
  double score;
  // Position of the matching text.
  // We currently use linear map, which will return one matching content, hence
  // the vector has only one element. When we have inverted index, we will have
  // multiple matching contents.
  std::vector<Position> positions;
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

  base::WeakPtrFactory<Index> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_INDEX_H_
