// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_MEDIA_RESOURCE_PROVIDER_SERVICE_H_
#define FUCHSIA_ENGINE_BROWSER_MEDIA_RESOURCE_PROVIDER_SERVICE_H_

#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace media {
class FuchsiaCdmManager;
}  // namespace media

class MediaResourceProviderService {
 public:
  MediaResourceProviderService();
  ~MediaResourceProviderService();

  MediaResourceProviderService(const MediaResourceProviderService&) = delete;
  MediaResourceProviderService& operator=(const MediaResourceProviderService&) =
      delete;

  void Bind(content::RenderFrameHost* frame_host,
            mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
                receiver);

 private:
  std::unique_ptr<media::FuchsiaCdmManager> cdm_manager_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_MEDIA_RESOURCE_PROVIDER_SERVICE_H_