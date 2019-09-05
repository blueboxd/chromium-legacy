// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_

#include "components/download/public/common/download_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace download {

// Interface for providing a SharedURLLoaderFactory on IO thread that can be
// used to create parallel download requests.
class COMPONENTS_DOWNLOAD_EXPORT URLLoaderFactoryProvider {
 public:
  URLLoaderFactoryProvider();
  virtual ~URLLoaderFactoryProvider();

  // Called on the io thread to get the URL loader.
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryProvider);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_LOADER_FACTORY_PROVIDER_H_
