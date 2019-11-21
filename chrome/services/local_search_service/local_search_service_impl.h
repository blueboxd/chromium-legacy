// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_
#define CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_

#include "base/macros.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace local_search_service {

class IndexImpl;

// Actual implementation of LocalSearchService.
// It creates and owns content-specific Indices. Clients can call it |GetIndex|
// method to get an Index for a given index id.
class LocalSearchServiceImpl : public mojom::LocalSearchService {
 public:
  LocalSearchServiceImpl();
  ~LocalSearchServiceImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::LocalSearchService> receiver);

  // mojom::LocalSearchService overrides.
  void GetIndex(mojom::LocalSearchService::IndexId index_id,
                mojo::PendingReceiver<mojom::Index> index) override;

 private:
  mojo::ReceiverSet<mojom::LocalSearchService> receivers_;
  std::map<mojom::LocalSearchService::IndexId, std::unique_ptr<IndexImpl>>
      indices_;

  DISALLOW_COPY_AND_ASSIGN(LocalSearchServiceImpl);
};

}  // namespace local_search_service

#endif  // CHROME_SERVICES_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_IMPL_H_
