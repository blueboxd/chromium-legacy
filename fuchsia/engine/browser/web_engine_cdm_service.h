// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_

#include <memory>

#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace media {
class FuchsiaCdmManager;
}

class WebEngineCdmService {
 public:
  WebEngineCdmService();
  WebEngineCdmService(const WebEngineCdmService&) = delete;
  WebEngineCdmService& operator=(const WebEngineCdmService&) = delete;
  ~WebEngineCdmService();

  void BindFuchsiaCdmProvider(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaCdmProvider> receiver);

 private:
  std::unique_ptr<media::FuchsiaCdmManager> cdm_manager_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_
