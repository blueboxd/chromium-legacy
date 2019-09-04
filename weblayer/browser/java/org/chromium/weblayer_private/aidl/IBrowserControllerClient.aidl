// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

/**
 * Interface used by BrowserController to inform the client of changes. This largely duplicates the
 *  BrowserObserver interface, but is a singleton to avoid unnecessary IPC.
 */
interface IBrowserControllerClient {
  /** The Uri that should be displayed in the url-bar has updated.  */
  void displayURLChanged(in String url) = 0;
}
