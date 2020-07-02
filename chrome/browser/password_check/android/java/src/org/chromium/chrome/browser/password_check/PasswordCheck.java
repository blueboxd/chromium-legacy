// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;

/**
 * This component allows to check for compromised passwords. It provides a settings page which shows
 * the compromised passwords and exposes actions that will help the users to make safer their
 * credentials.
 */
public interface PasswordCheck {
    /**
     * Initializes the PasswordCheck UI and launches it.
     * @param context A {@link Context} to create views and retrieve resources.
     */
    void showUi(Context context);
}
