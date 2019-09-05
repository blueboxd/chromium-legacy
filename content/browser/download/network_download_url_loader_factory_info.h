// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class URLLoaderFactoryGetter;

// Wrapper of a URLLoaderFactoryGetter that can be passed to another thread
// to retrieve URLLoaderFactory.
class NetworkDownloadURLLoaderFactoryInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  NetworkDownloadURLLoaderFactoryInfo(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter,
      network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info,
      network::mojom::URLLoaderFactoryRequest proxy_factory_request);
  ~NetworkDownloadURLLoaderFactoryInfo() override;

 protected:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

 private:
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory_;
  network::mojom::URLLoaderFactoryPtrInfo proxy_factory_ptr_info_;
  network::mojom::URLLoaderFactoryRequest proxy_factory_request_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDownloadURLLoaderFactoryInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_NETWORK_DOWNLOAD_URL_LOADER_FACTORY_INFO_H_
