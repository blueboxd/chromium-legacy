// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData() = 1;

  /**
   * Creates a new IBrowserFragmentController.
   * @param fragmentClient IRemoteFragmentClient that will host the Fragment implemented on the
   * weblayer side.
   * @param context Context that refers the the weblayer implementation
   */
  IBrowserFragmentController createBrowserFragmentController(
          in IRemoteFragmentClient fragmentClient, in IObjectWrapper context) = 2;
}
