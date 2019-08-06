// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_

#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrameHost;
}

class WebEngineCdmService {
 public:
  explicit WebEngineCdmService(
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
          registry);
  ~WebEngineCdmService();

 private:
  media::FuchsiaCdmManager cdm_manager_;

  // Not owned pointer. |registry_| must outlive |this|.
  service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>* const
      registry_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineCdmService);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CDM_SERVICE_H_
