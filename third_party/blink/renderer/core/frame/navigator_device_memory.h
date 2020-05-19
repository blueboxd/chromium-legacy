// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_DEVICE_MEMORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_DEVICE_MEMORY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class CORE_EXPORT NavigatorDeviceMemory : public GarbageCollectedMixin {
 public:
  explicit NavigatorDeviceMemory();
  explicit NavigatorDeviceMemory(Document* document);
  float deviceMemory() const;
  void Trace(Visitor*) const override;

 private:
  WeakMember<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_DEVICE_MEMORY_H_
