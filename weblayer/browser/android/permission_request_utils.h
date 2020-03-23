// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_
#define WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_

#include "base/callback_forward.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace weblayer {

using PermissionUpdatedCallback = base::OnceCallback<void(bool)>;

// Requests all necessary Android permissions related to
// |content_settings_type|, and calls |callback|. |callback| will be called with
// true if all permissions were successfully granted, and false otherwise.
void RequestAndroidPermission(content::WebContents* web_contents,
                              ContentSettingsType content_settings_type,
                              PermissionUpdatedCallback callback);

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ANDROID_PERMISSION_REQUEST_UTILS_H_
