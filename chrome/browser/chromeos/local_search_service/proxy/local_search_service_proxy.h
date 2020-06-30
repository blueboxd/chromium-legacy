// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_H_

#include <map>

#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace local_search_service {

class LocalSearchService;
class IndexProxy;

class LocalSearchServiceProxy : public mojom::LocalSearchServiceProxy,
                                public KeyedService {
 public:
  explicit LocalSearchServiceProxy(LocalSearchService* local_search_service);
  ~LocalSearchServiceProxy() override;

  LocalSearchServiceProxy(const LocalSearchServiceProxy&) = delete;
  LocalSearchServiceProxy& operator=(const LocalSearchServiceProxy) = delete;

  // mojom::LocalSearchServiceProxy:
  void GetIndex(
      IndexId index_id,
      Backend backend,
      mojo::PendingReceiver<mojom::IndexProxy> index_receiver) override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::LocalSearchServiceProxy> receiver);

 private:
  LocalSearchService* const service_;
  mojo::ReceiverSet<mojom::LocalSearchServiceProxy> receivers_;
  std::map<IndexId, std::unique_ptr<IndexProxy>> indices_;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_LOCAL_SEARCH_SERVICE_PROXY_H_
