// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

/**
 * Use {@link #createComponent()} to instantiate a {@link PasswordCheckComponent}.
 */
public class PasswordCheckComponentFactory {
    private PasswordCheckComponentFactory() {}

    /**
     * Creates a {@link PasswordCheckCoordinator}.
     * @return A {@link PasswordCheckCoordinator}.
     */
    public static PasswordCheckComponent createComponent() {
        return new PasswordCheckCoordinator();
    }
}