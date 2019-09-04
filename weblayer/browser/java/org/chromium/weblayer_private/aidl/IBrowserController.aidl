// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;

interface IBrowserController {
  void setClient(in IBrowserControllerClient client) = 0;

  void navigate(in String url) = 1;

  void setTopView(in IObjectWrapper view) = 2;

  void destroy() = 3;

  IObjectWrapper onCreateView() = 4;
}
