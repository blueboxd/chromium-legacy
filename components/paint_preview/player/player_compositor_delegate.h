// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gfx {
class Rect;
}  // namespace gfx

class SkBitmap;
class GURL;

namespace paint_preview {

class PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegate(PaintPreviewBaseService* paint_preview_service,
                           const GURL& url);

  virtual void OnCompositorReady(
      mojom::PaintPreviewCompositor::Status status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response) = 0;

  // Called when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to callback.
  void RequestBitmap(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      base::OnceCallback<void(mojom::PaintPreviewCompositor::Status,
                              const SkBitmap&)> callback);

  // Called on touch event on a frame.
  void OnClick(const base::UnguessableToken& frame_guid, int x, int y);

 protected:
  virtual ~PlayerCompositorDelegate();

 private:
  void OnCompositorServiceDisconnected();

  void OnCompositorClientCreated(const GURL& url);
  void OnCompositorClientDisconnected();

  PaintPreviewBaseService* paint_preview_service_;
  std::unique_ptr<PaintPreviewCompositorService>
      paint_preview_compositor_service_;
  std::unique_ptr<PaintPreviewCompositorClient>
      paint_preview_compositor_client_;

  base::WeakPtrFactory<PlayerCompositorDelegate> weak_factory_{this};

  PlayerCompositorDelegate(const PlayerCompositorDelegate&) = delete;
  PlayerCompositorDelegate& operator=(const PlayerCompositorDelegate&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
