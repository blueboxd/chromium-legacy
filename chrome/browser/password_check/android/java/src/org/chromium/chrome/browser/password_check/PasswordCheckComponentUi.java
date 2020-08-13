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
     * Implementers of this delegate are expected to launch apps or Chrome Custom tabs that enable
     * the user to change a compromised password.
     */
    interface ChangePasswordDelegate {
        /**
         * @param credential A {@link CompromisedCredential}.
         * @return True iff there is a valid URL to navigate to or an app that can be opened.
         */
        boolean canManuallyChangeCredential(CompromisedCredential credential);

        /**
         * Launches an app (if available) or a CCT with the site the given credential was used on.
         * @param credential A {@link CompromisedCredential}.
         */
        void launchAppOrCctWithChangePasswordUrl(CompromisedCredential credential);

        /**
         * Launches a CCT with the site the given credential was used on and invokes the script that
         * fixes the compromised credential automatically.
         * @param credential A {@link CompromisedCredential}.
         */
        void launchCctWithScript(CompromisedCredential credential);
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
     * Forwards the signal that the fragment is being destroyed.
     */
    void onDestroyFragment();

    /**
     * Tears down the component when it's no longer needed.
     */
    void destroy();
}
