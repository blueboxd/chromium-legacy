// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
#define CHROME_BROWSER_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace local_search_service {

class LocalSearchServiceImpl;

// This class owns an implementation of LocalSearchService.
// It exposes LocalSearchService through the mojo interface by returning a
// remote. However, in-process clients can request implementation ptr directly.
// TODO(jiameng): the next cl will remove mojo and will provide impl directly.
class LocalSearchServiceProxy : public KeyedService {
 public:
  // Profile isn't required, hence can be nullptr in tests.
  explicit LocalSearchServiceProxy(Profile* profile);
  ~LocalSearchServiceProxy() override;

  LocalSearchServiceProxy(const LocalSearchServiceProxy&) = delete;
  LocalSearchServiceProxy& operator=(const LocalSearchServiceProxy&) = delete;

  // Clients should call this function to get a remote to LocalSearchService.
  // This function returns to the caller a pointer to |remote_|, which is bound
  // to |local_search_service_impl_|.
  mojom::LocalSearchService* GetLocalSearchService();

  // For in-process clients, it could be more efficient to get the
  // implementation ptr directly.
  LocalSearchServiceImpl* GetLocalSearchServiceImpl();

 private:
  void CreateLocalSearchServiceAndBind();
  std::unique_ptr<LocalSearchServiceImpl> local_search_service_impl_;
  mojo::Remote<mojom::LocalSearchService> remote_;

  base::WeakPtrFactory<LocalSearchServiceProxy> weak_ptr_factory_{this};
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
