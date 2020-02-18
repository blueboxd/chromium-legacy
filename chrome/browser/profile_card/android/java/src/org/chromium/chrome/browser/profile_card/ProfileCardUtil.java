// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

/** Provide util functions for profile card component. */
public class ProfileCardUtil {
    ProfileCardUtil() {}

    /** Determines whether to show the profile card entry point. */
    public static boolean shouldShowEntryPoint() {
        // TODO(crbug/1053611): add logic about whether to show the profile card entry point.
        return false;
    }

    /** Talks to the backend and gets the profile card data. */
    public static ProfileCardData getProfileCardData() {
        // TODO(crbug/1053610): add logic that queries the profile card data.
        return null;
    }
}
