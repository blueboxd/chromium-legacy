// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;

interface IBrowserFragmentController {
  IProfile getProfile() = 0;
  IBrowserController getBrowserController() = 1;
  void setTopView(IObjectWrapper view) = 3;

  // |valueCallback| is a wrapped ValueCallback<Boolean> instead. The bool value in |valueCallback|
  // indicates is whether the request was successful. Request might fail if it is subsumed by a
  // following request, or if this object is destroyed.
  void setSupportsEmbedding(in boolean enable, in IObjectWrapper valueCallback) = 4;
}
