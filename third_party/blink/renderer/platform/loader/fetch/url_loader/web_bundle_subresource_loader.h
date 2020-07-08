// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_BUNDLE_SUBRESOURCE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_BUNDLE_SUBRESOURCE_LOADER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Creates a network::mojom::URLLoaderFactory that can load resources from a
// WebBundle, and binds it to |factory_receiver|.
PLATFORM_EXPORT void CreateWebBundleSubresourceLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
    mojo::ScopedDataPipeConsumerHandle bundle_body);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WEB_BUNDLE_SUBRESOURCE_LOADER_H_
