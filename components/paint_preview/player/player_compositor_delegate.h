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
#include "components/paint_preview/browser/hit_tester.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gfx {
class Rect;
}  // namespace gfx

class SkBitmap;

namespace paint_preview {

class DirectoryKey;

class PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegate(PaintPreviewBaseService* paint_preview_service,
                           const GURL& url,
                           const DirectoryKey& key,
                           bool skip_service_launch = false);
  virtual ~PlayerCompositorDelegate();

  PlayerCompositorDelegate(const PlayerCompositorDelegate&) = delete;
  PlayerCompositorDelegate& operator=(const PlayerCompositorDelegate&) = delete;

  virtual void OnCompositorReady(
      mojom::PaintPreviewCompositor::Status status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response) {}

  // Called when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to callback.
  void RequestBitmap(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      base::OnceCallback<void(mojom::PaintPreviewCompositor::Status,
                              const SkBitmap&)> callback);

  // Called on touch event on a frame.
  std::vector<const GURL*> OnClick(const base::UnguessableToken& frame_guid,
                                   const gfx::Rect& rect);

 private:
  void OnCompositorServiceDisconnected();

  void OnCompositorClientCreated(const GURL& expected_url,
                                 const DirectoryKey& key);
  void OnCompositorClientDisconnected();

  void OnProtoAvailable(const GURL& expected_url,
                        std::unique_ptr<PaintPreviewProto> proto);
  void SendCompositeRequest(
      mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request);

  PaintPreviewBaseService* paint_preview_service_;
  std::unique_ptr<PaintPreviewCompositorService>
      paint_preview_compositor_service_;
  std::unique_ptr<PaintPreviewCompositorClient>
      paint_preview_compositor_client_;
  base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>
      hit_testers_;

  base::WeakPtrFactory<PlayerCompositorDelegate> weak_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
