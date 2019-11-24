// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_SESSION_DESCRIPTION_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_SESSION_DESCRIPTION_PLATFORM_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class PLATFORM_EXPORT RTCSessionDescriptionPlatform final
    : public GarbageCollected<RTCSessionDescriptionPlatform> {
 public:
  RTCSessionDescriptionPlatform(const WebString& type, const WebString& sdp);

  WebString GetType() { return type_; }
  void SetType(const WebString& type) { type_ = type; }

  WebString Sdp() { return sdp_; }
  void SetSdp(const WebString& sdp) { sdp_ = sdp; }

  void Trace(blink::Visitor* visitor) {}

 private:
  WebString type_;
  WebString sdp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_SESSION_DESCRIPTION_PLATFORM_H_
