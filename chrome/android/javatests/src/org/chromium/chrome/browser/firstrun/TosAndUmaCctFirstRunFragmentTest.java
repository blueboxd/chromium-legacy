// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Test for first run activity and {@link TosAndUmaCctFirstRunFragment}.
 * For the outside signals that used in this test so that the verification is focusing on the
 * workflow and UI transition.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TosAndUmaCctFirstRunFragmentTest {
    @IntDef({FragmentState.LOADING, FragmentState.NO_POLICY, FragmentState.HAS_POLICY})
    @Retention(RetentionPolicy.SOURCE)
    @interface FragmentState {
        int LOADING = 0;
        int NO_POLICY = 1;
        int HAS_POLICY = 2;
    }

    @Rule
    public DisableAnimationsTestRule mDisableAnimationsTestRule = new DisableAnimationsTestRule();

    @Mock
    public FirstRunAppRestrictionInfo mMockAppRestrictionInfo;

    private FirstRunActivityTestObserver mTestObserver = new FirstRunActivityTestObserver();
    private FirstRunActivity mActivity;
    private TosAndUmaCctFirstRunFragment mFragment;

    private View mTosText;
    private View mAcceptButton;
    private View mLargeSpinner;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));

        // Static switches.
        FirstRunActivity.setEnableEnterpriseCCTForTest(true);
        FirstRunAppRestrictionInfo.setInstanceForTest(mMockAppRestrictionInfo);
        TosAndUmaCctFirstRunFragment.setBlockPolicyLoadingForTest(true);
    }

    @After
    public void tearDown() {
        FirstRunActivity.setEnableEnterpriseCCTForTest(false);
        FirstRunAppRestrictionInfo.setInstanceForTest(null);
        TosAndUmaCctFirstRunFragment.setBlockPolicyLoadingForTest(false);
        if (mActivity != null) mActivity.finish();
    }

    @Test
    @SmallTest
    public void testNoRestriction() {
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        TestThreadUtils.runOnUiThreadBlocking(() -> mFragment.onAppRestrictionDetected(false));
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
    public void testWithRestriction_NoPolicy() {
        setHasAppRestrictionForMock();
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        waitUntilNativeLoaded();

        // TODO(crbug.com/1108118): In our current test setup, #onCctPolicyDetected will actually
        //  called inside #onNativeInitialized. When we decouple these two signals, we should update
        //  the test accordingly.
        TestThreadUtils.runOnUiThreadBlocking(() -> mFragment.onCctTosPolicyDetected(true));
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
    public void testWithRestriction_WithPolicy() {
        setHasAppRestrictionForMock();
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        waitUntilNativeLoaded();

        TestThreadUtils.runOnUiThreadBlocking(() -> mFragment.onCctTosPolicyDetected(false));
        assertUIState(FragmentState.HAS_POLICY);

        Mockito.verify(mFragment).exitCctFirstRun();
    }

    /**
     * Launch chrome through custom tab and trigger first run.
     */
    private void launchFirstRunThroughCustomTab() {
        FirstRunActivity.setObserverForTest(mTestObserver);

        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        final Context context = instrumentation.getTargetContext();

        // Create an Intent that causes Chrome to run.
        Intent intent =
                CustomTabsTestUtils.createMinimalCustomTabIntent(context, "https://test.com");

        // Start the FRE.
        final ActivityMonitor freMonitor =
                new ActivityMonitor(FirstRunActivity.class.getName(), null, false);
        instrumentation.addMonitor(freMonitor);
        // As we want to test on FirstRunActivity, which starts its lifecycle *before*
        // CustomTabActivity fully initialized, we'll launch the activity without the help of
        // CustomTabActivityTestRule (which waits until any tab is created).
        context.startActivity(intent);

        // Wait for the FRE to be ready to use.
        Activity activity =
                freMonitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        instrumentation.removeMonitor(freMonitor);

        mActivity = (FirstRunActivity) activity;

        CriteriaHelper.pollUiThread(
                () -> mActivity.getSupportFragmentManager().getFragments().size() > 0);
        mFragment = Mockito.spy((TosAndUmaCctFirstRunFragment) mActivity.getSupportFragmentManager()
                                        .getFragments()
                                        .get(0));
        mTosText = mActivity.findViewById(R.id.tos_and_privacy);
        mAcceptButton = mActivity.findViewById(R.id.tos_and_privacy);
        mLargeSpinner = mActivity.findViewById(R.id.progress_spinner_large);
    }

    private void assertUIState(@FragmentState int fragmentState) {
        int tosVisibility = View.INVISIBLE;
        int spinnerVisibility = View.GONE;

        if (fragmentState == FragmentState.NO_POLICY) {
            tosVisibility = View.VISIBLE;
        }

        if (fragmentState == FragmentState.LOADING) {
            spinnerVisibility = View.VISIBLE;
        }

        Assert.assertEquals("Visibility of ToS text is different than the test setting.",
                tosVisibility, mTosText.getVisibility());
        Assert.assertEquals("Visibility of accept button is different than the test setting.",
                tosVisibility, mAcceptButton.getVisibility());
        Assert.assertEquals("Visibility of Loading spinner is different than the test setting.",
                spinnerVisibility, mLargeSpinner.getVisibility());
    }

    /** Set up mock FirstRunAppRestrictionInfo that there is app restriction on the device */
    private void setHasAppRestrictionForMock() {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(true);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());
    }

    private void waitUntilNativeLoaded() {
        CriteriaHelper.pollUiThread(
                (() -> mActivity.isNativeSideIsInitializedForTest()), "native never initialized.");
    }
}
