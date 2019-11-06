// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;

import java.util.List;

interface IBrowserFragmentController {
  IProfile getProfile() = 0;
  void setTopView(in IObjectWrapper view) = 1;

  // |valueCallback| is a wrapped ValueCallback<Boolean> instead. The bool value in |valueCallback|
  // indicates is whether the request was successful. Request might fail if it is subsumed by a
  // following request, or if this object is destroyed.
  void setSupportsEmbedding(in boolean enable, in IObjectWrapper valueCallback) = 2;

  // Returns false if browserController is not attached to this fragment.
  boolean setActiveBrowserController(in IBrowserController browserController) = 3;

  int getActiveBrowserControllerId() = 4;
  List getBrowserControllers() = 5;
}
