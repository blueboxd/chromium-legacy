// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

@RunWith(BaseJUnit4ClassRunner.class)
public class FragmentRestoreTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRotation() {
        mActivityTestRule.launchShellWithUrl("about:blank");

        String url = "data:text,foo";
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.waitForNavigation(url);

        mActivityTestRule.rotateActivity();

        url = "data:text,bar";
        mActivityTestRule.loadUrl(url);
        mActivityTestRule.waitForNavigation(url);
    }
}
