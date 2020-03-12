// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.privacy;

/**
 * TODO(chouinard): Remove this temporary wrapper class after downstream references are migrated.
 */
public class PrivacyPreferencesManager {
    public static org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager
    getInstance() {
        return org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager.getInstance();
    }
}
