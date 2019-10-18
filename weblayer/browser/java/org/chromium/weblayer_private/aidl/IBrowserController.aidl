// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IFullscreenDelegateClient;
import org.chromium.weblayer_private.aidl.INavigationController;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;

interface IBrowserController {
  void setClient(in IBrowserControllerClient client) = 0;

  INavigationController createNavigationController(in INavigationControllerClient client) = 1;

  void setDownloadDelegateClient(IDownloadDelegateClient client) = 2;

  void setFullscreenDelegateClient(in IFullscreenDelegateClient client) = 3;
}
