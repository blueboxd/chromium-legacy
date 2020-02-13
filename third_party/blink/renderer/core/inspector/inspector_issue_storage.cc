// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"

#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

static const unsigned kMaxIssueCount = 1000;

InspectorIssueStorage::InspectorIssueStorage() = default;

void InspectorIssueStorage::AddInspectorIssue(ExecutionContext* context,
                                              InspectorIssue* issue) {
  DCHECK(issues_.size() <= kMaxIssueCount);
  probe::InspectorIssueAdded(context, issue);
  if (issues_.size() == kMaxIssueCount) {
    issues_.pop_front();
  }
  issues_.push_back(issue);
}

void InspectorIssueStorage::Clear() {
  issues_.clear();
}

wtf_size_t InspectorIssueStorage::size() const {
  return issues_.size();
}

InspectorIssue* InspectorIssueStorage::at(wtf_size_t index) const {
  return issues_[index].Get();
}

void InspectorIssueStorage::Trace(Visitor* visitor) {
  visitor->Trace(issues_);
}

}  // namespace blink
