// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;

/**
 * Test rule that provides access to a Chrome application.
 */
public class ChromeUiApplicationTestRule extends ExternalResource {
    // TODO(aluo): Adjust according to https://crrev.com/c/1585142.
    private static final String PACKAGE_NAME_ARG = "PackageUnderTest";

    private String mPackageName;

    /**
     * Returns an instance of the page controller that corresponds to the current page.
     * @param controllers      List of possible page controller instances to search among.
     * @return                 The detected page controller.
     * @throws UiLocationError If page can't be determined.
     */
    public static PageController detectPageAmong(PageController... controllers) {
        for (PageController instance : controllers) {
            if (instance.isCurrentPageThis()) return instance;
        }
        throw UiLocationException.newInstance("Could not detect current Page");
    }

    /**
     * Launch the Chrome application.
     */
    public void launchApplication() {
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();
        utils.launchApplication(mPackageName);
    }

    public String getApplicationPackage() {
        return mPackageName;
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        mPackageName = InstrumentationRegistry.getArguments().getString(PACKAGE_NAME_ARG);
        // If the runner didn't pass the package name under test to us, then we can assume
        // that the target package name provided in the AndroidManifest is the app-under-test.
        if (mPackageName == null) {
            mPackageName = InstrumentationRegistry.getTargetContext().getPackageName();
        }
    }
}
