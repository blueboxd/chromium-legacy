// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/image_annotation/public/mojom/constants.mojom-forward.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chrome {
namespace internal {

// Forward image Annotator requests to the image_annotation service.
void BindImageAnnotator(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  content::BrowserContext::GetConnectorFor(
      frame_host->GetProcess()->GetBrowserContext())
      ->BindInterface(
          image_annotation::mojom::kServiceName,
          image_annotation::mojom::AnnotatorRequest(std::move(receiver)));
}

void PopulateChromeFrameBinders(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));
}

}  // namespace internal
}  // namespace chrome
