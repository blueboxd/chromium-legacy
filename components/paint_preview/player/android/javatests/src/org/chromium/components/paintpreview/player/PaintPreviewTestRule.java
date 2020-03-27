// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

/**
 * Loads native and initializes the browser process for Paint Preview instrumentation tests.
 */
public class PaintPreviewTestRule extends NativeLibraryTestRule {
    private FakeAccountManagerDelegate mAccountManager;

    /**
     * {@link AccountManagerFacadeProvider#getInstance()} is called in the browser initialization
     * path. If we don't mock {@link AccountManagerFacade}, we'll run into a failed assertion.
     */
    private void setUp() {
        mAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.DISABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacadeProvider.overrideAccountManagerFacadeForTests(mAccountManager);
        loadNativeLibraryAndInitBrowserProcess();
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
            }
        }, description);
    }
}
