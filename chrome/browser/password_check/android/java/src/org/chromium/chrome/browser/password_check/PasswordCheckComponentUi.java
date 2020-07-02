// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.view.MenuItem;

/**
 * This component is responsible for handling the UI logic for the password check.
 */
interface PasswordCheckComponentUi {
    /**
     * Handle the request of the user to show the help page for the Check Passwords view.
     */
    public boolean handleHelp(MenuItem item);
}
