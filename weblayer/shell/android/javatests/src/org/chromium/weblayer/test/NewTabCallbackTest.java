// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that NewTabCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NewTabCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @SmallTest
    public void testNewBrowser() {
        String url = mActivityTestRule.getTestDataURL("new_browser.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        callback.waitForNewTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, mActivity.getBrowser().getTabs().size());
            Tab secondTab = mActivity.getBrowser().getActiveTab();
            Assert.assertNotSame(firstTab, secondTab);
        });
    }
}
