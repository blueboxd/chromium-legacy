// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue.h"

#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_inspector_issue.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

InspectorIssue::InspectorIssue(mojom::blink::InspectorIssueCode code)
    : code_(code) {}

InspectorIssue::~InspectorIssue() = default;

InspectorIssue* InspectorIssue::Create(mojom::blink::InspectorIssueCode code) {
  return MakeGarbageCollected<InspectorIssue>(code);
}

const mojom::blink::InspectorIssueCode& InspectorIssue::Code() const {
  return code_;
}

void InspectorIssue::Trace(Visitor* visitor) {}

}  // namespace blink
