// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_

#include "base/component_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_message_center {

// Creates a string describing media session metadata intended to be read out by
// a screen reader.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
base::string16 GetAccessibleNameFromMetadata(
    media_session::MediaMetadata session_metadata);

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
