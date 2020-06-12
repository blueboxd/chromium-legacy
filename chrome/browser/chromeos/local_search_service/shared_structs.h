// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

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
// These numbers are used for logging and should not be changed or reused. More
// will be added later.
enum class ResponseStatus {
  kUnknownError = 0,
  // Search operation is successful. But there could be no matching item and
  // result list is empty.
  kSuccess = 1,
  // Query is empty.
  kEmptyQuery = 2,
  // Index is empty (i.e. no data).
  kEmptyIndex = 3
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_SHARED_STRUCTS_H_
