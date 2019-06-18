// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.design.widget.CoordinatorLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBox;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxCoordinator;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxModel;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the Autofill Assistant infobox.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillAssistantInfoBoxUiTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    private TextView getExplanationView(AssistantInfoBoxCoordinator coordinator) {
        return coordinator.getView().findViewById(R.id.info_box_explanation);
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantInfoBoxCoordinator createCoordinator(AssistantInfoBoxModel model) {
        ThreadUtils.assertOnUiThread();

        Bitmap testImage =
                BitmapFactory.decodeResource(getActivity().getResources(), R.drawable.btn_close);

        AssistantInfoBoxCoordinator coordinator =
                new AssistantInfoBoxCoordinator(InstrumentationRegistry.getTargetContext(), model,
                        new AutofillAssistantUiTestUtil.MockImageFetcher(testImage, null));

        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;

        ViewGroup chromeCoordinatorView = getActivity().findViewById(R.id.coordinator);
        chromeCoordinatorView.addView(coordinator.getView(), lp);

        return coordinator;
    }

    /** Tests assumptions about the initial state of the infobox. */
    @Test
    @SmallTest
    public void testInitialState() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantInfoBoxModel model = new AssistantInfoBoxModel();
            AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

            assertNull(model.get(AssistantInfoBoxModel.INFO_BOX));
            assertFalse(coordinator.getView().isShown());
        });
    }

    /** Tests for an infobox with a message, but without an image. */
    @Test
    @SmallTest
    public void testMessageNoImage() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantInfoBoxModel model = new AssistantInfoBoxModel();
            AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

            AssistantInfoBox infoBox = new AssistantInfoBox("", "Message");
            model.set(AssistantInfoBoxModel.INFO_BOX, infoBox);

            assertTrue(getExplanationView(coordinator).isShown());
            assertNull("Image should not be set",
                    getExplanationView(coordinator).getCompoundDrawables()[1]);
            assertEquals(infoBox.getExplanation(), getExplanationView(coordinator).getText());
        });
    }

    /** Tests for an infobox with message and image. */
    @Test
    @SmallTest
    public void testImage() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantInfoBoxModel model = new AssistantInfoBoxModel();
            AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

            AssistantInfoBox infoBox = new AssistantInfoBox("x", "Message");
            model.set(AssistantInfoBoxModel.INFO_BOX, infoBox);

            assertTrue(getExplanationView(coordinator).isShown());
            assertNotNull("Image should be set",
                    getExplanationView(coordinator).getCompoundDrawables()[1]);
            assertEquals(infoBox.getExplanation(), getExplanationView(coordinator).getText());
        });
    }

    @Test
    @SmallTest
    public void testVisibility() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantInfoBoxModel model = new AssistantInfoBoxModel();
            AssistantInfoBoxCoordinator coordinator = createCoordinator(model);

            AssistantInfoBox infoBox = new AssistantInfoBox("", "");
            model.set(AssistantInfoBoxModel.INFO_BOX, infoBox);
            assertTrue(coordinator.getView().isShown());

            model.set(AssistantInfoBoxModel.INFO_BOX, null);
            assertFalse(coordinator.getView().isShown());
        });
    }
}
