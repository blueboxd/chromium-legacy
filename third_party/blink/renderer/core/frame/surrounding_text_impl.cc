// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/surrounding_text_impl.h"

#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_surrounding_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SurroundingTextImpl::SurroundingTextImpl(WebLocalFrameImpl& frame,
                                         InterfaceRegistry* interface_registry)
    : frame_(&frame) {
  if (!interface_registry)
    return;
  // TODO(crbug.com/800641): Use InterfaceValidator when it works for associated
  // interfaces.
  interface_registry->AddAssociatedInterface(WTF::BindRepeating(
      &SurroundingTextImpl::BindToReceiver, WrapWeakPersistent(this)));
}

void SurroundingTextImpl::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::SurroundingText> receiver) {
  receiver_.Bind(std::move(receiver),
                 frame_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void SurroundingTextImpl::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  blink::WebSurroundingText surrounding_text(frame_, max_length);

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
