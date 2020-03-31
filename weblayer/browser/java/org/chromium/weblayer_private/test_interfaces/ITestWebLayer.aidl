// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test_interfaces;

interface ITestWebLayer {
  // Force network connectivity state.
  boolean isNetworkChangeAutoDetectOn() = 1;
  // set mock location provider
  void setMockLocationProvider(in boolean enable) = 2;
  boolean isMockLocationProviderRunning() = 3;

  // Whether or not a permission dialog is currently showing.
  boolean isPermissionDialogShown() = 4;

  // Clicks a button on the permission dialog.
  void clickPermissionDialogButton(in boolean allow) = 5;
}
