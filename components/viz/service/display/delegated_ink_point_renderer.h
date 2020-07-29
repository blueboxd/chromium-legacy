// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_H_

#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/delegated_ink_point.mojom.h"

namespace viz {

// This class is used for rendering delegated ink trails on the end of strokes
// to reduce user perceived latency. On initialization, it binds the mojo
// interface required for receiving delegated ink points that are made and sent
// from the browser process.
// TODO(1052145): Expand on this comment as more functionality is added to this
//   function - it will ultimately be where the rendering actually occurs.
//
// For more information on the feature, please see the explainer:
// https://github.com/WICG/ink-enhancement/blob/master/README.md
class VIZ_SERVICE_EXPORT DelegatedInkPointRendererImpl
    : public mojom::DelegatedInkPointRenderer {
 public:
  explicit DelegatedInkPointRendererImpl(
      mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver);
  ~DelegatedInkPointRendererImpl() override;
  DelegatedInkPointRendererImpl(const DelegatedInkPointRendererImpl&) = delete;
  DelegatedInkPointRendererImpl& operator=(
      const DelegatedInkPointRendererImpl&) = delete;

  void StoreDelegatedInkPoint(const DelegatedInkPoint& point) override;

 private:
  mojo::Receiver<mojom::DelegatedInkPointRenderer> receiver_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_H_
