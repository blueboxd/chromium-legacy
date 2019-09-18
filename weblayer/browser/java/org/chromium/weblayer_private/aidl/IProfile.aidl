// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData() = 1;

  /**
   * Creates a new IBrowserController.
   * @param clientContext Context from the client
   * @param implContext Context that refers to the weblayer implementation
   */
  IBrowserController createBrowserController(in IObjectWrapper clientContext,
                                             in IObjectWrapper implContext) = 2;
}
