// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_LOCAL_SEARCH_SERVICE_INDEX_IMPL_H_
#define CHROME_SERVICES_LOCAL_SEARCH_SERVICE_INDEX_IMPL_H_

#include "base/macros.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace local_search_service {

// Actual implementation of a local search service Index.
// It has a registry of searchable data, which can be updated. It also runs an
// asynchronous search function to find matching items for a given query, and
// returns results via a callback.
class IndexImpl : public mojom::Index {
 public:
  IndexImpl();
  ~IndexImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Index> receiver);

  // mojom::Index overrides.
  void GetSize(GetSizeCallback callback) override;

  void AddOrUpdate(std::vector<mojom::DataPtr> data,
                   AddOrUpdateCallback callback) override;

  void Delete(const std::vector<std::string>& ids,
              DeleteCallback callback) override;

  void Find(const std::string& query,
            int32_t max_latency_in_ms,
            int32_t max_results,
            FindCallback callback) override;

 private:
  // Returns all search results for a given query.
  std::vector<mojom::ResultPtr> GetSearchResults(
      const std::string& query) const;

  // A map from key to search-tags.
  std::map<std::string, std::vector<std::string>> data_;

  mojo::ReceiverSet<mojom::Index> receivers_;

  DISALLOW_COPY_AND_ASSIGN(IndexImpl);
};

}  // namespace local_search_service

#endif  // CHROME_SERVICES_LOCAL_SEARCH_SERVICE_INDEX_IMPL_H_
