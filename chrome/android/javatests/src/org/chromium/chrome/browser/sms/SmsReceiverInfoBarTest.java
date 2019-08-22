// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the SmsReceiverInfoBar class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SmsReceiverInfoBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarOk() throws TimeoutException, InterruptedException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        SmsReceiverInfoBar infoBar =
                SmsReceiverInfoBar.create(/*enumeratedIconId=*/0, "title", "message", "ok");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> InfoBarContainer.get(tab).addInfoBarForTesting(infoBar));

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Click primary button.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarClose() throws TimeoutException, InterruptedException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        SmsReceiverInfoBar infoBar =
                SmsReceiverInfoBar.create(/*enumeratedIconId=*/0, "title", "message", "ok");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> InfoBarContainer.get(tab).addInfoBarForTesting(infoBar));

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Close infobar.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));
    }
}
