// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss;

/** An interface for a callbacks related to dismisses so proper logging can occur. */
public interface PendingDismissCallback {
    /* Called when the dismiss has been reverted and the dismissed content has come back. */
    void onDismissReverted();

    /* Called when the dismiss has been committed and can no longer be reversed. */
    void onDismissCommitted();
}
