// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_

#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace mojom {
enum class PushErrorType;
enum class PushRegistrationStatus;
}  // namespace mojom

class PushSubscriptionOptions;

WTF::String PushRegistrationStatusToString(
    mojom::PushRegistrationStatus status);

mojom::PushErrorType PushRegistrationStatusToPushErrorType(
    mojom::PushRegistrationStatus status);

// Converts a {blink::PushSubscriptionOptions} object into a
// {blink::mojom::blink::PushSubscriptionOptions} object. Since the buffer of
// the former may be bigger than the capacity of the latter, a caller of this
// function has to guarantee that the ByteLength of the input buffer fits into
// {wtf_size_t} range.
blink::mojom::blink::PushSubscriptionOptionsPtr
ConvertSubscriptionOptionPointer(blink::PushSubscriptionOptions* input);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
