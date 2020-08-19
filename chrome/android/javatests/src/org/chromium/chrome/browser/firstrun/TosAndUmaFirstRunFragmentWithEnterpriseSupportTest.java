// Copyright 2020 The Chromium Authors. All rights reserved.
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

import org.hamcrest.Matchers;
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
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.policy.PolicyService;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Test for first run activity and {@link TosAndUmaFirstRunFragmentWithEnterpriseSupport}.
 * For the outside signals that used in this test so that the verification is focusing on the
 * workflow and UI transition.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TosAndUmaFirstRunFragmentWithEnterpriseSupportTest {
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
    @Mock
    public PolicyService mPolicyService;
    @Mock
    public FirstRunUtils.Natives mFirstRunUtils;

    private FirstRunActivityTestObserver mTestObserver = new FirstRunActivityTestObserver();
    private FirstRunActivity mActivity;
    private TosAndUmaFirstRunFragmentWithEnterpriseSupport mFragment;
    private final List<PolicyService.Observer> mPolicyServiceObservers = new ArrayList<>();
    private final List<Callback<Boolean>> mAppRestrictonsCallbacks = new ArrayList<>();

    private View mTosText;
    private View mAcceptButton;
    private View mLargeSpinner;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Assert.assertFalse(
                CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE));

        FirstRunActivity.setEnableEnterpriseCCTForTest(true);
        FirstRunAppRestrictionInfo.setInstanceForTest(mMockAppRestrictionInfo);
        PolicyServiceFactory.setPolicyServiceForTest(mPolicyService);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
    }

    @After
    public void tearDown() {
        FirstRunActivity.setEnableEnterpriseCCTForTest(false);
        FirstRunAppRestrictionInfo.setInstanceForTest(null);
        PolicyServiceFactory.setPolicyServiceForTest(null);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
        if (mActivity != null) mActivity.finish();
    }

    @Test
    @SmallTest
    public void testNoRestriction() {
        setAppRestrictiosnMockNotInitialized();
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        TestThreadUtils.runOnUiThreadBlocking(() -> setAppRestrictiosnMockInitialized(false));
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
    public void testWithRestriction_DialogEnabled() {
        setAppRestrictiosnMockInitialized(true);
        setPolicyServiceMockNotInitialized();
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setMockCctTosDialogEnabled(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> setPolicyServiceMockInitialized());
        assertUIState(FragmentState.NO_POLICY);
    }

    @Test
    @SmallTest
    public void testWithRestriction_DialogDisabled() {
        setAppRestrictiosnMockInitialized(true);
        setPolicyServiceMockNotInitialized();
        launchFirstRunThroughCustomTab();
        assertUIState(FragmentState.LOADING);

        setMockCctTosDialogEnabled(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> setPolicyServiceMockInitialized());

        assertUIState(FragmentState.HAS_POLICY);
        // TODO(https://crbug.com/1113229): Rework this to not depend on {@link FirstRunActivity}
        // implementation details.
        Assert.assertTrue(FirstRunStatus.isEphemeralSkipFirstRun());
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
        mFragment = (TosAndUmaFirstRunFragmentWithEnterpriseSupport) mActivity
                            .getSupportFragmentManager()
                            .getFragments()
                            .get(0);

        // Force this to happen now to try to make the tests more deterministic. Ideally the tests
        // could control when this happens and test for difference sequences.
        waitUntilNativeLoaded();

        mTosText = mActivity.findViewById(R.id.tos_and_privacy);
        mAcceptButton = mActivity.findViewById(R.id.tos_and_privacy);
        mLargeSpinner = mActivity.findViewById(R.id.progress_spinner_large);
    }

    private void assertUIState(@FragmentState int fragmentState) {
        int tosVisibility =
                (fragmentState == FragmentState.NO_POLICY) ? View.VISIBLE : View.INVISIBLE;
        int spinnerVisibility = (fragmentState == FragmentState.LOADING) ? View.VISIBLE : View.GONE;

        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                "Visibility of Loading spinner never reached test setting.",
                                mLargeSpinner.getVisibility(), Matchers.is(spinnerVisibility)));

        Assert.assertEquals("Visibility of ToS text is different than the test setting.",
                tosVisibility, mTosText.getVisibility());
        Assert.assertEquals("Visibility of accept button is different than the test setting.",
                tosVisibility, mAcceptButton.getVisibility());
    }

    private void waitUntilNativeLoaded() {
        CriteriaHelper.pollUiThread(
                (() -> mActivity.isNativeSideIsInitializedForTest()), "native never initialized.");
    }

    private void setAppRestrictiosnMockNotInitialized() {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   mAppRestrictonsCallbacks.add(callback);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());
    }

    private void setAppRestrictiosnMockInitialized(boolean hasAppRestrictons) {
        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   callback.onResult(hasAppRestrictons);
                   return null;
               })
                .when(mMockAppRestrictionInfo)
                .getHasAppRestriction(any());

        for (Callback<Boolean> callback : mAppRestrictonsCallbacks) {
            callback.onResult(hasAppRestrictons);
        }
    }

    private void setPolicyServiceMockNotInitialized() {
        Mockito.when(mPolicyService.isInitializationComplete()).thenReturn(false);
        Mockito.doAnswer(invocation -> {
                   PolicyService.Observer observer = invocation.getArgument(0);
                   mPolicyServiceObservers.add(observer);
                   return null;
               })
                .when(mPolicyService)
                .addObserver(any());
    }

    private void setPolicyServiceMockInitialized() {
        Mockito.when(mPolicyService.isInitializationComplete()).thenReturn(true);
        for (PolicyService.Observer observer : mPolicyServiceObservers) {
            observer.onPolicyServiceInitialized();
        }
        mPolicyServiceObservers.clear();
    }

    private void setMockCctTosDialogEnabled(boolean cctTosDialogEnabled) {
        Mockito.when(mFirstRunUtils.getCctTosDialogEnabled()).thenReturn(cctTosDialogEnabled);
    }
}
