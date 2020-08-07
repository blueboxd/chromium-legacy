// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_

// A delegate which exposes browser functionality from //chrome to the camera
// app ui page handler.
class CameraAppUIDelegate {
 public:
  virtual ~CameraAppUIDelegate() = default;

  // Sets Downloads folder as launch directory by File Handling API so that we
  // can get the handle on the app side.
  virtual void SetLaunchDirectory() = 0;
};

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
