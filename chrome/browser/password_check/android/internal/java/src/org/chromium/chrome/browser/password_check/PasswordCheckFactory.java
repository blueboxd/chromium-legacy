// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

/**
 * Use {@link #create()} to instantiate a {@link PasswordCheckImpl}.
 */
public class PasswordCheckFactory {
    private static PasswordCheck sPasswordCheck;
    private PasswordCheckFactory() {}

    /**
     * Creates a {@link PasswordCheckImpl}.
     * @return A {@link PasswordCheckImpl}.
     */
    public static PasswordCheck create() {
        if (sPasswordCheck == null) {
            sPasswordCheck = new PasswordCheckImpl();
        }
        return sPasswordCheck;
    }
}