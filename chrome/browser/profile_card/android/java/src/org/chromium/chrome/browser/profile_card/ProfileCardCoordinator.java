// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

/** Interface for the Profile Card related UI. */
public interface ProfileCardCoordinator {

    /**
     * Shows the profile card.
     */
    void show();

    /**
     * Hides the profile card.
     */
    void hide();
}
