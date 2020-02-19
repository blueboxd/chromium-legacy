// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.graphics.Bitmap;

/** Interface for the Profile Card entry point UI. */
public interface EntryPointCoordinator {
    /**
     * Initiates the entry point coordinator.
     */
    void init(Context context, Bitmap bitmap);

    /**
     * Shows the profile card entry point.
     */
    void show();

    /**
     * Hides the profile card entry point.
     */
    void hide();
}
