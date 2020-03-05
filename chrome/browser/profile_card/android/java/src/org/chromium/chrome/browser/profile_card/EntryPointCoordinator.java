// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;

import org.chromium.content_public.browser.NavigationHandle;

/** Interface for the Profile Card entry point UI. */
public interface EntryPointCoordinator {
    /**
     * Initiates the entry point coordinator.
     */
    void init(Context context);

    /**
     * Shows the profile card entry point if the condition is fulfilled.
     * @param navigationHandle {@link NavigationHandle} to fetch the data.
     */
    void maybeShow(NavigationHandle navigationHandle);

    /**
     * Hides the profile card entry point.
     */
    void hide();
}
