// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

/**
 * Provides information about a navigation.
 */
interface INavigation {
  int getState() = 0;

  String getUri() = 1;

  List<String> getRedirectChain() = 2;
}
