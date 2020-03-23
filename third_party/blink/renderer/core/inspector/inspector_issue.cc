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

InspectorIssue::InspectorIssue(mojom::blink::InspectorIssueCode code,
                               mojom::blink::InspectorIssueDetailsPtr details,
                               mojom::blink::AffectedResourcesPtr resources)
    : code_(code),
      details_(std::move(details)),
      resources_(std::move(resources)) {
  DCHECK(details_);
  DCHECK(resources_);
}

InspectorIssue::~InspectorIssue() = default;

InspectorIssue* InspectorIssue::Create(
    mojom::blink::InspectorIssueInfoPtr info) {
  DCHECK(info->details);
  DCHECK(info->resources);
  return MakeGarbageCollected<InspectorIssue>(
      info->code, std::move(info->details), std::move(info->resources));
}

mojom::blink::InspectorIssueCode InspectorIssue::Code() const {
  return code_;
}

const mojom::blink::InspectorIssueDetailsPtr& InspectorIssue::Details() const {
  return details_;
}

const mojom::blink::AffectedResourcesPtr& InspectorIssue::Resources() const {
  return resources_;
}

void InspectorIssue::Trace(blink::Visitor* visitor) {}

}  // namespace blink
