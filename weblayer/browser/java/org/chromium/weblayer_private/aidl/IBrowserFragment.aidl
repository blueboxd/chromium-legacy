// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IRemoteFragment;

interface IBrowserFragment {
  IRemoteFragment asRemoteFragment() = 0;
  IBrowserFragmentController getController() = 1;
}
