// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_

#include "base/optional.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink-forward.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"

namespace cc {
class PaintCanvas;
}

namespace blink {
class IntRect;
class LocalFrame;
class MessageEvent;
class ResourceRequest;
class SecurityOrigin;
class WebLocalFrame;

class RemoteFrameClient : public FrameClient {
 public:
  ~RemoteFrameClient() override = default;

  virtual void Navigate(const ResourceRequest&,
                        blink::WebLocalFrame* initiator_frame,
                        bool should_replace_current_entry,
                        bool is_opener_navigation,
                        bool initiator_frame_has_download_sandbox_flag,
                        bool initiator_frame_is_ad,
                        mojo::PendingRemote<mojom::blink::BlobURLToken>,
                        const base::Optional<WebImpression>& impression) = 0;
  unsigned BackForwardLength() override = 0;

  // Forwards a postMessage for a remote frame.
  virtual void ForwardPostMessage(
      MessageEvent*,
      scoped_refptr<const SecurityOrigin> target,
      base::Optional<base::UnguessableToken> cluster_id,
      LocalFrame* source_frame) const = 0;

  // Forwards a change to the rects of a remote frame. |local_frame_rect| is the
  // size of the frame in its parent's coordinate space prior to applying CSS
  // transforms. |screen_space_rect| is in the screen's coordinate space, after
  // CSS transforms are applied.
  virtual void FrameRectsChanged(const IntRect& local_frame_rect,
                                 const IntRect& screen_space_rect) = 0;

  virtual void UpdateRemoteViewportIntersection(
      const ViewportIntersectionState& intersection_state) = 0;

  virtual void AdvanceFocus(mojom::blink::FocusType, LocalFrame* source) = 0;

  virtual uint32_t Print(const IntRect&, cc::PaintCanvas*) const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REMOTE_FRAME_CLIENT_H_
