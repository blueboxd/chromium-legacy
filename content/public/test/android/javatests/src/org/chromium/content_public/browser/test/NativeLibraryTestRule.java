// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * TestRule that adds support for loading and dealing with native libraries.
 *
 * NativeLibraryTestRule does not interact with any Activity.
 *
 * TODO(crbug.com/1096666): Remove this class in favor of using {@link NativeLibraryTestUtils}
 * directly.
 */
@Deprecated
public class NativeLibraryTestRule implements TestRule {
    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     */
    public void loadNativeLibraryNoBrowserProcess() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    /**
     * Loads the native library on the activity UI thread (must not be called from the UI thread).
     * After loading the library, this will initialize the browser process.
     */
    public void loadNativeLibraryAndInitBrowserProcess() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return base;
    }
}
