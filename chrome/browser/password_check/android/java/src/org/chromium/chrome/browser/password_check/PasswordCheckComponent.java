// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;

/**
 * This component allows to check for passwords leak. It launches a settings page which shows the
 * leaked passwords and exposes actions that will help the users to make safer their credentials.
 */
public interface PasswordCheckComponent {
    /**
     * Initialize the component.
     * @param context A {@link Context} to create views and retrieve resources.
     */
    void initialize(Context context);
}
