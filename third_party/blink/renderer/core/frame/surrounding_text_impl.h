// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/surrounding_text.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalFrame;

// Implementation of mojom::blink::SurroundingText
class CORE_EXPORT SurroundingTextImpl final
    : public GarbageCollectedFinalized<SurroundingTextImpl>,
      public Supplement<LocalFrame>,
      public mojom::blink::SurroundingText {
  USING_GARBAGE_COLLECTED_MIXIN(SurroundingTextImpl);

 public:
  static const char kSupplementName[];

  static void BindToReceiver(
      LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver);

  static SurroundingTextImpl* From(LocalFrame* frame);

  explicit SurroundingTextImpl(
      LocalFrame& frame,
      mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver);
  ~SurroundingTextImpl() override;

  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) final;

 private:
  mojo::AssociatedReceiver<mojom::blink::SurroundingText> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(SurroundingTextImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SURROUNDING_TEXT_IMPL_H_
