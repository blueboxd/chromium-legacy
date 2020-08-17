// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.view.MenuItem;

/**
 * This component is responsible for handling the UI logic for the password check.
 */
public interface PasswordCheckComponentUi {
    /**
     * A delegate that handles native tasks for the UI component.
     */
    interface Delegate {
        /**
         * Remove the given credential from the password store.
         * @param credential A {@link CompromisedCredential}.
         */
        void removeCredential(CompromisedCredential credential);
    }

    /**
     * Handle the request of the user to show the help page for the Check Passwords view.
     * @param item A {@link MenuItem}.
     */
    boolean handleHelp(MenuItem item);

    /**
     * Forwards the signal that the fragment was started.
     */
    void onStartFragment();

    /**
     * Forwards the signal that the fragment is being resumed.
     */
    void onResumeFragment();

    /**
     * Forwards the signal that the fragment is being destroyed.
     */
    void onDestroyFragment();

    /**
     * Tears down the component when it's no longer needed.
     */
    void destroy();
}
