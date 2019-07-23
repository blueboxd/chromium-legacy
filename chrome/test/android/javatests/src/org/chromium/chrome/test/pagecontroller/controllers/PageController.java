// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers;

import android.os.RemoteException;

import org.junit.Assert;

/**
 * Base class for page controllers.
 * A page controller allows tests to interact with a single page (think Android activity)
 * in the app-under-test.
 */
public abstract class PageController extends ElementController {
    public void pressAndroidBackButton() {
        mUtils.pressBack();
    }

    public void pressAndroidHomeButton() {
        mUtils.pressHome();
    }

    public void pressAndroidOverviewButton() {
        try {
            // UiDevice (used by UiAutomatorUtils) calls this the recent apps button,
            // Android UI seems to prefer the overview button name (as evidenced by talkback's
            // readout)
            mUtils.pressRecentApps();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Checks if the current page displayed corresponds to this page controller.
     * @return True if current page can be controlled by this controller, else false.
     */
    public abstract boolean isCurrentPageThis();

    /**
     * Verifies that the current page belongs to the controller.
     * @throws           AssertionError if the current page does not belong the controller.
     */
    public void verify() {
        Assert.assertTrue("Page expected to be " + getClass().getName(), isCurrentPageThis());
    }
}
