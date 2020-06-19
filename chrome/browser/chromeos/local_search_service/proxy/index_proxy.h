// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_INDEX_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_INDEX_PROXY_H_

#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/chromeos/local_search_service/proxy/local_search_service_proxy.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace local_search_service {

class Index;

class IndexProxy : public mojom::IndexProxy {
 public:
  explicit IndexProxy(Index* index);
  ~IndexProxy() override;

  void BindReceiver(mojo::PendingReceiver<mojom::IndexProxy> receiver);

  // mojom::IndexProxy:
  void GetSize(GetSizeCallback callback) override;
  void AddOrUpdate(const std::vector<Data>& data,
                   AddOrUpdateCallback callback) override;
  void Delete(const std::vector<std::string>& ids,
              DeleteCallback callback) override;
  void Find(const base::string16& query,
            uint32_t max_results,
            FindCallback callback) override;

 private:
  Index* const index_;
  mojo::ReceiverSet<mojom::IndexProxy> receivers_;
};

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_PROXY_INDEX_PROXY_H_
