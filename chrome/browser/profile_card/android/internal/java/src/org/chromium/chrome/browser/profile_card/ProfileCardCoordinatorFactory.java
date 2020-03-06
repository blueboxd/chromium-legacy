// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.view.View;

/** The factory for creating a {@link ProfileCardCoordinator}. */
public class ProfileCardCoordinatorFactory {
    ProfileCardCoordinatorFactory() {}

    /**
     * Create a new profile card coordinator.
     * @param view The {@link View} triggers the profile card.
     * @param CreatorMetadata The {@link CreatorMetadata} stores all data needed by profile card.
     */
    public static ProfileCardCoordinator createProfileCardCoordinator(
            View anchorView, CreatorMetadata creatorMetadata) {
        return new ProfileCardCoordinatorImpl(anchorView, creatorMetadata);
    }
}
