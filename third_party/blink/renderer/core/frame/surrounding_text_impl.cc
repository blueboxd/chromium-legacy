// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/surrounding_text_impl.h"

#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
const char SurroundingTextImpl::kSupplementName[] = "SurroundingTextImpl";

// static
void SurroundingTextImpl::BindToReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver) {
  if (!frame)
    return;
  frame->ProvideSupplement(
      MakeGarbageCollected<SurroundingTextImpl>(*frame, std::move(receiver)));
}

// static
SurroundingTextImpl* SurroundingTextImpl::From(LocalFrame* frame) {
  if (!frame)
    return nullptr;
  return frame->RequireSupplement<SurroundingTextImpl>();
}

SurroundingTextImpl::SurroundingTextImpl(
    LocalFrame& frame,
    mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver)
    : Supplement<LocalFrame>(frame),
      receiver_(this,
                std::move(receiver),
                frame.GetTaskRunner(blink::TaskType::kInternalDefault)) {}

SurroundingTextImpl::~SurroundingTextImpl() = default;

void SurroundingTextImpl::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  blink::SurroundingText surrounding_text(GetSupplementable(), max_length);

  if (surrounding_text.IsEmpty()) {
    // |surrounding_text| might not be correctly initialized, for example if
    // |frame_->SelectionRange().IsNull()|, in other words, if there was no
    // selection.
    std::move(callback).Run(String(), 0, 0);
    return;
  }

  std::move(callback).Run(surrounding_text.TextContent(),
                          surrounding_text.StartOffsetInTextContent(),
                          surrounding_text.EndOffsetInTextContent());
}

}  // namespace blink
