// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/surrounding_text.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class WebLocalFrameImpl;

// Implementation of mojom::blink::SurroundingText
class CORE_EXPORT SurroundingTextImpl final
    : public GarbageCollectedFinalized<SurroundingTextImpl>,
      public mojom::blink::SurroundingText {
 public:
  SurroundingTextImpl(WebLocalFrameImpl& frame,
                      InterfaceRegistry* interface_registry);

  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver);

  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) final;

  void Trace(blink::Visitor* visitor) { visitor->Trace(frame_); }

 private:
  const Member<WebLocalFrameImpl> frame_;

  mojo::AssociatedReceiver<mojom::blink::SurroundingText> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(SurroundingTextImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_
