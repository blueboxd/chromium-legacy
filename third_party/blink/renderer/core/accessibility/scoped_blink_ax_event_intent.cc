// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"

#include "base/macros.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

namespace blink {

ScopedBlinkAXEventIntent::ScopedBlinkAXEventIntent(
    const BlinkAXEventIntent& intent,
    Document* document)
    : intent_(intent), document_(document) {
  DCHECK(document_);
  DCHECK(document_->IsActive());
  if (AXObjectCache* cache = document_->ExistingAXObjectCache()) {
    AXObjectCache::BlinkAXEventIntentsSet& active_intents =
        cache->ActiveEventIntents();
    active_intents.insert(intent);
  }
}

ScopedBlinkAXEventIntent::~ScopedBlinkAXEventIntent() {
  // If a conservative GC is required, |document_| may become nullptr.
  if (!document_ || !document_->IsActive())
    return;

  if (AXObjectCache* cache = document_->ExistingAXObjectCache()) {
    AXObjectCache::BlinkAXEventIntentsSet& active_intents =
        cache->ActiveEventIntents();
    DCHECK(active_intents.Contains(intent_));
    active_intents.erase(intent_);
  }
}

}  // namespace blink
