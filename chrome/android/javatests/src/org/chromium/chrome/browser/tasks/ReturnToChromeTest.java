// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS;

import android.app.Activity;
import android.content.Context;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.concurrent.TimeoutException;

/**
 * Tests the functionality of return to chrome features that open overview mode if the timeout
 * has passed.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ReturnToChromeTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();
    }

    /**
     * Test that overview mode is not triggered if the delay is longer than the interval between
     * stop and start.
     */
    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:" + TAB_SWITCHER_ON_RETURN_MS
                    + "/100000"})
    public void
    testObserverModeNotTriggeredWithoutDelay() throws Exception {
        finishActivityCompletely();

        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();

        Assert.assertFalse(mActivity.getLayoutManager().overviewVisible());
    }

    @Test
    @SmallTest
    @Feature({"ReturnToChrome"})
    @DisabledTest(message = "crbug.com/955436")
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
            "enable-features=" + ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:" + TAB_SWITCHER_ON_RETURN_MS + "/1"})
    public void
    testObserverModeTriggeredWithDelay() throws Exception {
        finishActivityCompletely();

        // Sleep past the timeout.
        SystemClock.sleep(30);

        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();

        Assert.assertTrue(mActivity.getLayoutManager().overviewVisible());
    }

    private void finishActivityCompletely() throws InterruptedException, TimeoutException {
        final CallbackHelper activityCallback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener stateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState == ActivityState.STOPPED) {
                            activityCallback.notifyCalled();
                            ApplicationStatus.unregisterActivityStateListener(this);
                        }
                    }
                };

        ApplicationStatus.registerStateListenerForAllActivities(stateListener);
        try {
            mActivity.finish();
            activityCallback.waitForCallback("Activity did not stop as expected", 0);
            mActivityTestRule.setActivity(null);
        } finally {
            ApplicationStatus.unregisterActivityStateListener(stateListener);
        }
    }
}
