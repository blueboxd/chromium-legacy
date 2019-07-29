// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.empty;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.directactions.DirectActionReporter;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Definition;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Type;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests the direct actions exposed by AA. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class AutofillAssistantDirectActionHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeActivity.class);

    private ChromeActivity mActivity;
    private BottomSheetController mBottomSheetController;
    private DirectActionHandler mHandler;
    private TestingAutofillAssistantModuleEntryProvider mModuleEntryProvider;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();

        mBottomSheetController = ThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiTestUtil.createBottomSheetController(mActivity));
        mModuleEntryProvider = new TestingAutofillAssistantModuleEntryProvider();
        mModuleEntryProvider.setCannotInstall();

        mHandler = new AutofillAssistantDirectActionHandler(mActivity, mBottomSheetController,
                mActivity.getScrim(), mActivity.getTabModelSelector()::getCurrentTab,
                mModuleEntryProvider);

        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(AutofillAssistantPreferencesUtil.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED)
                .remove(AutofillAssistantPreferencesUtil.AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN)
                .apply();
    }

    @Test
    @MediumTest
    public void testReportAvailableDirectActions() throws Exception {
        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        mHandler.reportAvailableDirectActions(reporter);

        assertEquals(2, reporter.mActions.size());

        FakeDirectActionDefinition list = reporter.mActions.get(0);
        assertEquals("list_assistant_actions", list.mId);
        assertEquals(2, list.mParameters.size());
        assertEquals("user_name", list.mParameters.get(0).mName);
        assertEquals(Type.STRING, list.mParameters.get(0).mType);
        assertEquals(false, list.mParameters.get(0).mRequired);
        assertEquals("experiment_ids", list.mParameters.get(1).mName);
        assertEquals(Type.STRING, list.mParameters.get(1).mType);
        assertEquals(false, list.mParameters.get(1).mRequired);

        assertEquals(1, list.mResults.size());
        assertEquals("names", list.mResults.get(0).mName);
        assertEquals(Type.STRING, list.mResults.get(0).mType);

        FakeDirectActionDefinition perform = reporter.mActions.get(1);
        assertEquals("perform_assistant_action", perform.mId);
        assertEquals(2, perform.mParameters.size());
        assertEquals("name", perform.mParameters.get(0).mName);
        assertEquals(Type.STRING, perform.mParameters.get(0).mType);
        assertEquals("experiment_ids", perform.mParameters.get(1).mName);
        assertEquals(Type.STRING, perform.mParameters.get(1).mType);
        assertEquals(1, perform.mResults.size());
        assertEquals("success", perform.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, perform.mResults.get(0).mType);
    }

    @Test
    @MediumTest
    public void testOnboarding() throws Exception {
        mModuleEntryProvider.setInstalled();

        assertThat(listActions(), contains("onboarding"));

        acceptOnboarding();

        assertTrue(AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted());
    }

    @Test
    @MediumTest
    public void testModuleNotAvailable() throws Exception {
        mModuleEntryProvider.setCannotInstall();

        assertThat(listActions(), contains("onboarding"));
        assertFalse(performAction("onboarding", Bundle.EMPTY));
    }

    @Test
    @MediumTest
    public void testInstallModuleOnDemand() throws Exception {
        mModuleEntryProvider.setNotInstalled();

        assertThat(listActions(), contains("onboarding"));
        acceptOnboarding();
    }

    @Test
    @MediumTest
    public void testSwitchedOffInPreferences() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);

        assertThat(listActions(), empty());
        assertFalse(performAction("onboarding", Bundle.EMPTY));
    }

    private void acceptOnboarding() throws Exception {
        WaitingCallback<Boolean> onboardingCallback =
                performActionAsync("onboarding", Bundle.EMPTY);

        assertFalse(onboardingCallback.hasResult());
        onView(withId(R.id.button_init_ok)).perform(click());
        assertEquals(Boolean.TRUE, onboardingCallback.waitForResult("accept onboarding"));
    }

    // TODO(b/134741524): Add tests that list and execute direct actions coming from scripts, once
    // we have a way to fake RPCs and can create a bottom sheet controller on demand.

    /** Calls list_assistant_actions and returns the result. */
    private List<String> listActions() throws Exception {
        WaitingCallback<Bundle> callback = new WaitingCallback<Bundle>();
        assertTrue(ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mHandler.performDirectAction(
                                "list_assistant_actions", Bundle.EMPTY, callback)));
        return Arrays.asList(TextUtils.split(
                callback.waitForResult("list_assistant_actions").getString("names", ""), ","));
    }

    /** Calls perform_assistant_action and returns the result. */
    private Boolean performAction(String name, Bundle arguments) throws Exception {
        return performActionAsync(name, arguments).waitForResult("perform_assistant_action");
    }

    /**
     * Calls perform_assistant_action and returns a {@link WaitingCallback} that'll eventually
     * contain the result.
     */
    private WaitingCallback<Boolean> performActionAsync(String name, Bundle arguments)
            throws Exception {
        WaitingCallback<Boolean> callback = new WaitingCallback<Boolean>();
        Bundle allArguments = new Bundle(arguments);
        if (!name.isEmpty()) allArguments.putString("name", name);
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mHandler.performDirectAction("perform_assistant_action", allArguments,
                                (bundle) -> callback.onResult(bundle.getBoolean("success"))));
        return callback;
    }

    /**
     * A callback that allows waiting for the result to be available, then retrieving it.
     */
    private static class WaitingCallback<T> implements Callback<T> {
        private final CallbackHelper mHelper = new CallbackHelper();
        private boolean mHasResult;
        private T mResult;

        @Override
        public synchronized void onResult(T result) {
            mResult = result;
            mHasResult = true;
            mHelper.notifyCalled();
        }

        synchronized T waitForResult(String msg) throws Exception {
            if (!mHasResult) mHelper.waitForCallback(msg);
            assertTrue(mHasResult);
            return mResult;
        }

        synchronized boolean hasResult() {
            return mHasResult;
        }

        synchronized T getResult() {
            return mResult;
        }
    }

    /**
     * An implementation of DirectActionReporter that just keeps the definitions in a field.
     *
     * <p>TODO(b/134740534) There should be shared test utilities for that.
     */
    private static class FakeDirectActionReporter implements DirectActionReporter {
        List<FakeDirectActionDefinition> mActions = new ArrayList<>();

        @Override
        public DirectActionReporter.Definition addDirectAction(String actionId) {
            FakeDirectActionDefinition action = new FakeDirectActionDefinition(actionId);
            mActions.add(action);
            return action;
        }

        @Override
        public void report() {}
    }

    /** A simple action definition for testing. */
    private static class FakeDirectActionDefinition implements Definition {
        final String mId;
        List<FakeParameter> mParameters = new ArrayList<>();
        List<FakeParameter> mResults = new ArrayList<>();

        FakeDirectActionDefinition(String id) {
            mId = id;
        }

        @Override
        public Definition withParameter(String name, @Type int type, boolean required) {
            mParameters.add(new FakeParameter(name, type, required));
            return this;
        }

        @Override
        public Definition withResult(String name, @Type int type) {
            mResults.add(new FakeParameter(name, type, true));
            return this;
        }
    }

    /** A simple parameter definition for testing. */
    private static class FakeParameter {
        final String mName;

        @Type
        final int mType;
        final boolean mRequired;

        FakeParameter(String name, @Type int type, boolean required) {
            mName = name;
            mType = type;
            mRequired = required;
        }
    }
}
