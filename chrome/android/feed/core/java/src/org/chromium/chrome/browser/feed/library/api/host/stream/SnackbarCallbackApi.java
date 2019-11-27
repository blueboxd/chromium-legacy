// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

/** The SnackbarCallbackApi is a Callback class for Snackbar events. */
public interface SnackbarCallbackApi {
    /** Called when the user clicks the action button on the snackbar. */
    void onDismissedWithAction();

    /** Called when the snackbar is dismissed by timeout or UI environment change. */
    void onDismissNoAction();
}
