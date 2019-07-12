// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.support.annotation.IdRes;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests {@link AssistantOnboardingCoordinator}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class AssistantOnboardingCoordinatorTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Callback<Boolean> mCallback;

    private ChromeActivity mActivity;
    private BottomSheetController mBottomSheetController;
    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        AutofillAssistantUiTestUtil.startOnBlankPage(mCustomTabActivityTestRule);
        mActivity = mCustomTabActivityTestRule.getActivity();
        mBottomSheetController = ThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiTestUtil.createBottomSheetController(mActivity));
        mTab = mActivity.getTabModelSelector().getCurrentTab();
    }

    private AssistantOnboardingCoordinator createCoordinator(Tab tab) {
        AssistantOnboardingCoordinator coordinator =
                new AssistantOnboardingCoordinator("", mActivity, mBottomSheetController, mTab);
        coordinator.disableAnimationForTesting();
        return coordinator;
    }

    @Test
    @MediumTest
    public void testAcceptOnboarding() throws Exception {
        testOnboarding(R.id.button_init_ok, true);
    }

    @Test
    @MediumTest
    public void testRejectOnboarding() throws Exception {
        testOnboarding(R.id.button_init_not_ok, false);
    }

    private void testOnboarding(@IdRes int buttonToClick, boolean expectAccept) throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(!expectAccept);

        AssistantOnboardingCoordinator coordinator = createCoordinator(mTab);

        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.show(mCallback));

        assertTrue(ThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));
        onView(is(mActivity.getScrim())).check(matches(isDisplayed()));
        onView(withId(buttonToClick)).perform(click());

        verify(mCallback).onResult(expectAccept);

        assertFalse(ThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));
        assertEquals(expectAccept, AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted());
    }

    @Test
    @MediumTest
    public void testOnboardingWithNoTabs() throws Exception {
        AssistantOnboardingCoordinator coordinator = createCoordinator(/* tab= */ null);

        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.show(mCallback));

        onView(withId(R.id.button_init_ok)).perform(click());

        verify(mCallback).onResult(true);
    }

    @Test
    @MediumTest
    public void testTransfertControls() throws Exception {
        AssistantOnboardingCoordinator coordinator = createCoordinator(mTab);

        List<AssistantOverlayCoordinator> capturedOverlays =
                Collections.synchronizedList(new ArrayList<>());
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.show((accepted) -> {
            capturedOverlays.add(coordinator.transferControls());
        }));

        onView(withId(R.id.button_init_ok)).perform(click());
        assertFalse(ThreadUtils.runOnUiThreadBlocking(coordinator::isInProgress));

        // An overlay was captured, and it is still shown.
        onView(is(mActivity.getScrim())).check(matches(isDisplayed()));
        assertEquals(1, capturedOverlays.size());
        AssistantOverlayCoordinator overlay = capturedOverlays.get(0);
        assertNotNull(overlay);
        assertEquals(
                AssistantOverlayState.FULL, overlay.getModel().get(AssistantOverlayModel.STATE));

        // The bottom sheet content is still the assistant one.
        assertThat(mBottomSheetController.getBottomSheet().getCurrentSheetContent(),
                instanceOf(AssistantBottomSheetContent.class));
    }
}
