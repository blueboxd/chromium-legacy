// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.store;

/** Allows classes to listen to changes to {@link Store} */
public interface StoreListener {
    /**
     * Called when {@link Store} switches to ephemeral mode. This will be called on a background
     * thread.
     */
    void onSwitchToEphemeralMode();
}
