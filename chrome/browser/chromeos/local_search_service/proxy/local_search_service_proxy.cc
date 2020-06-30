// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy.h"

#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/chromeos/local_search_service/proxy/index_proxy.h"

namespace local_search_service {

LocalSearchServiceProxy::LocalSearchServiceProxy(
    local_search_service::LocalSearchService* local_search_service)
    : service_(local_search_service) {
  DCHECK(service_);
}

LocalSearchServiceProxy::~LocalSearchServiceProxy() = default;

void LocalSearchServiceProxy::GetIndex(
    IndexId index_id,
    Backend backend,
    mojo::PendingReceiver<mojom::IndexProxy> index_receiver) {
  auto it = indices_.find(index_id);
  if (it == indices_.end()) {
    Index* index = service_->GetIndex(index_id, backend);
    it = indices_.emplace(index_id, std::make_unique<IndexProxy>(index)).first;
  }
  it->second->BindReceiver(std::move(index_receiver));
}

void LocalSearchServiceProxy::BindReceiver(
    mojo::PendingReceiver<mojom::LocalSearchServiceProxy> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace local_search_service
