// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;

@RunWith(BaseJUnit4ClassRunner.class)
public class FragmentRestoreTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    @Test
    @SmallTest
    @DisabledTest
    public void successfullyLoadsUrlAfterRotation() {
        mActivityTestRule.launchShellWithUrl("about:blank");

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        mActivityTestRule.rotateActivity();

        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(url);
    }
}
