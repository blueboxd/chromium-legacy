// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;

interface IWebLayer {
  IProfile createProfile(in String path) = 0;

  // Initializes WebLayer and starts loading. It is expected that is called
  // before anything else. |loadedCallback| is a ValueCallback that is called
  // when load completes. |webLayerContext| is a Context that refers to the
  // WebLayer implementation.
  void initAndLoadAsync(in IObjectWrapper webLayerContext,
                        in IObjectWrapper loadedCallback) = 1;

  // Blocks until loading has completed.
  void loadSync() = 2;
}
