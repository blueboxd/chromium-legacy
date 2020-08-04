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
};

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_DELEGATE_H_
