// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import android.os.Bundle;

import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IBrowserFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;

interface IWebLayer {
  // Initializes WebLayer and starts loading. It is expected that is called
  // before anything else. |loadedCallback| is a ValueCallback that is called
  // when load completes. |appContext| is a Context that refers to the
  // Application using WebLayer.
  void initAndLoadAsync(in IObjectWrapper appContext,
                        in IObjectWrapper loadedCallback,
                        int resourcesPackageId) = 1;

  // Blocks until loading has completed.
  void loadSync() = 2;

  // Creates the WebLayer counterpart to a BrowserFragment - a BrowserFragmentImpl
  //
  // @param fragmentClient Representative of the Fragment on the client side through which
  // WebLayer can call methods on Fragment.
  // @param fragmentArgs Bundle of arguments with which the Fragment was created on the client side
  // (see Fragment#setArguments).
  IBrowserFragment createBrowserFragmentImpl(in IRemoteFragmentClient fragmentClient,
      in IObjectWrapper fragmentArgs) = 3;
}
