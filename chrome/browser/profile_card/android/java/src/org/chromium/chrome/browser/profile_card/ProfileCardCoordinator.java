// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.view.View;

import org.json.JSONObject;

/** Interface for the Profile Card related UI. */
public interface ProfileCardCoordinator {
    /**
     * Updates the {@link ProfileCard}
     * @param view {@link View} triggers the profile card.
     * @param profileCardData {@link JSONObject} stores all data needed by profile card.
     */
    void update(View view, JSONObject profileCardData);

    /**
     * Shows the profile card drop-down bubble.
     */
    void show();
}
