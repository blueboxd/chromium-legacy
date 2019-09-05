// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/url_loader_factory_provider.h"

namespace download {

URLLoaderFactoryProvider::URLLoaderFactoryProvider() = default;

URLLoaderFactoryProvider::~URLLoaderFactoryProvider() = default;

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryProvider::GetURLLoaderFactory() {
  return nullptr;
}

}  // namespace download
