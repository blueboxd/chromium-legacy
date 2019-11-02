// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.shell.InstrumentationActivity;

@RunWith(BaseJUnit4ClassRunner.class)
public class FragmentRestoreTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRotation() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        BrowserController controller = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowserController());

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(controller, url, false);

        mActivityTestRule.recreateActivity();

        InstrumentationActivity newActivity = mActivityTestRule.getActivity();
        controller = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> newActivity.getBrowserController());
        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(controller, url, false);
    }
}
