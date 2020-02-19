// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.view.View;

/** Interface for the Profile Card related UI. */
public interface ProfileCardCoordinator {
    /**
     * Initiates the profile card coordinator.
     * @param view {@link View} triggers the profile card.
     * @param profileCardData {@link ProfileCardData} stores all data needed by profile card.
     */
    void init(View view, ProfileCardData profileCardData);

    /**
     * Shows the profile card.
     */
    void show();

    /**
     * Hides the profile card.
     */
    void hide();
}
