// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import org.chromium.chrome.browser.profiles.Profile;

import javax.inject.Inject;

/**
 * Wrapper for the Profile class that allows for easier mocking.
 */
public class ProfileProvider {
    @Inject
    public ProfileProvider() {}

    public Profile getLastUsedRegularProfile() {
        return Profile.getLastUsedRegularProfile();
    }

    public Profile getPrimaryOTRProfile() {
        return Profile.getLastUsedRegularProfile().getPrimaryOTRProfile();
    }
}