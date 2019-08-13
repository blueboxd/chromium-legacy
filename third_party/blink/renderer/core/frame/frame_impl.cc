// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_impl.h"

#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
const char FrameImpl::kSupplementName[] = "FrameImpl";

// static
void FrameImpl::BindToReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver) {
  if (!frame)
    return;
  frame->ProvideSupplement(
      MakeGarbageCollected<FrameImpl>(*frame, std::move(receiver)));
}

// static
FrameImpl* FrameImpl::From(LocalFrame* frame) {
  if (!frame)
    return nullptr;
  return frame->RequireSupplement<FrameImpl>();
}

FrameImpl::FrameImpl(
    LocalFrame& frame,
    mojo::PendingAssociatedReceiver<mojom::blink::Frame> receiver)
    : Supplement<LocalFrame>(frame),
      receiver_(this,
                std::move(receiver),
                frame.GetTaskRunner(blink::TaskType::kInternalDefault)) {}

FrameImpl::~FrameImpl() = default;

void FrameImpl::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  blink::SurroundingText surrounding_text(GetSupplementable(), max_length);

  // |surrounding_text| might not be correctly initialized, for example if
  // |frame_->SelectionRange().IsNull()|, in other words, if there was no
  // selection.
  if (surrounding_text.IsEmpty()) {
    // Don't use WTF::String's default constructor so that we make sure that we
    // always send a valid empty string over the wire instead of a null pointer.
    std::move(callback).Run(g_empty_string, 0, 0);
    return;
  }

  std::move(callback).Run(surrounding_text.TextContent(),
                          surrounding_text.StartOffsetInTextContent(),
                          surrounding_text.EndOffsetInTextContent());
}

}  // namespace blink
