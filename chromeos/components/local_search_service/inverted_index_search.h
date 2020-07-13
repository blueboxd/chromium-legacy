// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/shared_structs.h"

namespace chromeos {
namespace local_search_service {

class InvertedIndex;

// A search via the inverted index backend with TF-IDF based document ranking.
class InvertedIndexSearch {
 public:
  InvertedIndexSearch();
  ~InvertedIndexSearch();

  InvertedIndexSearch(const InvertedIndexSearch&) = delete;
  InvertedIndexSearch& operator=(const InvertedIndexSearch&) = delete;

  // Returns number of data items.
  uint64_t GetSize();

  // Adds or updates data.
  // IDs of data should not be empty.
  void AddOrUpdate(const std::vector<Data>& data, bool build_index = true);

  // Deletes data with |ids| and returns number of items deleted.
  // If an id doesn't exist in the InvertedIndexSearch, no operation will be
  // done. IDs should not be empty.
  uint32_t Delete(const std::vector<std::string>& ids, bool build_index = true);

  // Returns matching results for a given query.
  // Zero |max_results| means no max.
  ResponseStatus Find(const base::string16& query,
                      uint32_t max_results,
                      std::vector<Result>* results);

  // Returns document id and number of occurrences of |term|.
  // Document ids are sorted in alphabetical order.
  std::vector<std::pair<std::string, uint32_t>> FindTermForTesting(
      const base::string16& term) const;

 private:
  std::unique_ptr<InvertedIndex> inverted_index_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
