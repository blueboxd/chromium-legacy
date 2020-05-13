// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_
#define CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class WebUIExtensionData
    : public RenderFrameObserver,
      public RenderFrameObserverTracker<WebUIExtensionData>,
      public mojom::WebUI {
 public:
  static void Create(RenderFrame* render_frame,
                     mojo::PendingReceiver<mojom::WebUI> receiver);
  explicit WebUIExtensionData(RenderFrame* render_frame);
  ~WebUIExtensionData() override;

  // Returns value for a given |key|. Will return an empty string if no such key
  // exists in the |variable_map_|.
  std::string GetValue(const std::string& key) const;

  void SendMessage(const std::string& message,
                   std::unique_ptr<base::ListValue> args);

 private:
  // mojom::WebUI
  void SetProperty(const std::string& name, const std::string& value) override;

  // RenderFrameObserver
  void OnDestruct() override {}

  std::map<std::string, std::string> variable_map_;

  mojo::Remote<mojom::WebUIHost> remote_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebUIExtensionData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_EXTENSION_DATA_H_
