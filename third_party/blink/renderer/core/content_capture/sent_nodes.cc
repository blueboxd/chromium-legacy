// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/sent_nodes.h"

#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {

bool SentNodes::HasSent(const Node& node) {
  return sent_nodes_.Contains(&node);
}

void SentNodes::OnSent(const Node& node) {
  sent_nodes_.insert(WeakMember<const Node>(&node));
}

void SentNodes::Trace(Visitor* visitor) {
  visitor->Trace(sent_nodes_);
}

}  // namespace blink
