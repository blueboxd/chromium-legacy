// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IProfile {
  void destroy() = 0;

  void clearBrowsingData(in int[] dataTypes, long fromMillis, long toMillis,
          in IObjectWrapper completionCallback) = 1;

  String getName() = 2;

  void setDownloadDirectory(String directory) = 3;

  // Added in Version 82.
  void destroyAndDeleteDataFromDisk(in IObjectWrapper completionCallback) = 4;

  // Added in Version 83.
  void setDownloadCallbackClient(IDownloadCallbackClient client) = 5;

  // Added in Version 83.
  ICookieManager getCookieManager() = 6;
}
