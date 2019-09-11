// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

class WebEngineContentRendererClient : public content::ContentRendererClient {
 public:
  WebEngineContentRendererClient();
  ~WebEngineContentRendererClient() override;

  // content::ContentRendererClient overrides.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems)
      override;
  bool IsSupportedVideoType(const media::VideoType& type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebEngineContentRendererClient);
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
