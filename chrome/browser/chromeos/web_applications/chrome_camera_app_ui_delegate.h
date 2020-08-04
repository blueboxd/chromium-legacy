// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include "chromeos/components/camera_app_ui/camera_app_ui_delegate.h"

/**
 * Implementation of the CameraAppUIDelegate interface. Provides the camera app
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeCameraAppUIDelegate : public CameraAppUIDelegate {
 public:
  ChromeCameraAppUIDelegate();

  ChromeCameraAppUIDelegate(const ChromeCameraAppUIDelegate&) = delete;
  ChromeCameraAppUIDelegate& operator=(const ChromeCameraAppUIDelegate&) =
      delete;
};

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_CAMERA_APP_UI_DELEGATE_H_
