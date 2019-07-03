// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers;

import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

/**
 * Base class for a Page Controller or Page Element Controller.
 * Allows Controllers to perform UI actions and verify UI state.
 */
public class ElementController {
    protected final UiAutomatorUtils mUtils;
    protected final UiLocatorHelper mLocatorHelper;

    public ElementController() {
        mUtils = UiAutomatorUtils.getInstance();
        mLocatorHelper = mUtils.getLocatorHelper();
    }
}
