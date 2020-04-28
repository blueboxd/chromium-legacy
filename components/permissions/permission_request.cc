// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace permissions {

PermissionRequest::PermissionRequest() {}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionRequestGestureType::UNKNOWN;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  return ContentSettingsType::DEFAULT;
}

base::string16 PermissionRequest::GetMessageTextWarningFragment() const {
  return base::string16();
}

GURL PermissionRequest::GetEmbeddingOrigin() const {
  NOTREACHED();
  return GURL();
}

#if defined(OS_ANDROID)
base::string16 PermissionRequest::GetQuietTitleText() const {
  return base::string16();
}

base::string16 PermissionRequest::GetQuietMessageText() const {
  return GetMessageText();
}
#endif

}  // namespace permissions
