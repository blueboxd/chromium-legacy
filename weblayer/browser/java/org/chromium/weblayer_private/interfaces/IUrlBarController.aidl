// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

interface IUrlBarController {

  // Deprecated in 84, use createUrlBarView with three arguments.
  IObjectWrapper /* View */ deprecatedCreateUrlBarView(in Bundle options) = 0;

  // Since 86
  IObjectWrapper /* View */ createUrlBarView(
      in Bundle options,
      in IObjectWrapper /* View.OnClickListener */ textClickListener,
      in IObjectWrapper /* View.OnLongClickListener */ textLongClickListener) = 1;
}
