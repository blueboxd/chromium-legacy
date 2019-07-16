// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_

#include <memory>
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

// Supports the event reporting between Blink WebAudio module and the
// associated DevTool's inspector agent. Generates an UUID for each element,
// and keeps an UUID for a parent element.
class InspectorHelperMixin {
 public:
  explicit InspectorHelperMixin(const String& parent_uuid)
      : uuid_(WTF::CreateCanonicalUUIDString()), parent_uuid_(parent_uuid) {}
  ~InspectorHelperMixin() = default;

  const String& Uuid() const { return uuid_; }
  const String& ParentUuid() const { return parent_uuid_; }

 private:
  const String uuid_;
  const String parent_uuid_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_INSPECTOR_HELPER_MIXIN_H_
