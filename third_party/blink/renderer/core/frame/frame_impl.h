// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalFrame;

// Implementation of mojom::blink::Frame
class CORE_EXPORT FrameImpl final : public GarbageCollected<FrameImpl>,
                                    public Supplement<LocalFrame>,
                                    public mojom::blink::Frame {
  USING_GARBAGE_COLLECTED_MIXIN(FrameImpl);

 public:
  static const char kSupplementName[];

  static void BindToReceiver(
      LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver);

  static FrameImpl* From(LocalFrame* frame);

  explicit FrameImpl(
      LocalFrame& frame,
      mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver);
  ~FrameImpl() override;

  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) final;

 private:
  mojo::AssociatedReceiver<mojom::blink::Frame> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_IMPL_H_
