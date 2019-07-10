// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isTextMaxLines;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsCoordinator;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Calendar;
import java.util.Locale;

/** Tests for the Autofill Assistant details. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantDetailsUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static class ViewHolder {
        final ImageView mImageView;
        final TextView mTitleView;
        final TextView mDescriptionLine1View;
        final TextView mDescriptionLine2View;
        final TextView mDescriptionLine3View;
        final TextView mPriceAttributionView;
        final View mPriceView;
        final TextView mTotalPriceLabelView;
        final TextView mTotalPriceView;

        ViewHolder(View detailsView) {
            mImageView = detailsView.findViewById(R.id.details_image);
            mTitleView = detailsView.findViewById(R.id.details_title);
            mDescriptionLine1View = detailsView.findViewById(R.id.details_line1);
            mDescriptionLine2View = detailsView.findViewById(R.id.details_line2);
            mDescriptionLine3View = detailsView.findViewById(R.id.details_line3);
            mPriceAttributionView = detailsView.findViewById(R.id.details_price_attribution);
            mPriceView = detailsView.findViewById(R.id.details_price);
            mTotalPriceView = detailsView.findViewById(R.id.details_total_price);
            mTotalPriceLabelView = detailsView.findViewById(R.id.details_total_price_label);
        }
    }

    private AssistantDetailsCoordinator createCoordinator(AssistantDetailsModel model)
            throws Exception {
        return createCoordinator(model, Locale.getDefault());
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantDetailsCoordinator createCoordinator(
            AssistantDetailsModel model, Locale locale) throws Exception {
        AssistantDetailsCoordinator coordinator = runOnUiThreadBlocking(() -> {
            Bitmap testImage = BitmapFactory.decodeResource(
                    mTestRule.getActivity().getResources(), R.drawable.btn_close);

            return new AssistantDetailsCoordinator(InstrumentationRegistry.getTargetContext(),
                    locale, model,
                    new AutofillAssistantUiTestUtil.MockImageFetcher(testImage, null));
        });

        runOnUiThreadBlocking(()
                                      -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                              mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
    }

    /** Tests assumptions about the initial state of the details. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);

        assertThat(model.get(AssistantDetailsModel.DETAILS), nullValue());
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
    }

    /** Tests visibility of views. */
    @Test
    @MediumTest
    public void testVisibility() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        runOnUiThreadBlocking(() -> {
            model.set(AssistantDetailsModel.DETAILS,
                    new AssistantDetails("title", "image", null, false, "Total", "$12",
                            java.util.Calendar.getInstance().getTime(), "line 1", "line 2",
                            "line 3", false, false, false, false, false, false));
        });

        onView(is(viewHolder.mImageView)).check(matches(isDisplayed()));
        onView(is(viewHolder.mTitleView)).check(matches(isDisplayed()));
        onView(is(viewHolder.mDescriptionLine1View)).check(matches(isDisplayed()));
        onView(is(viewHolder.mDescriptionLine2View)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPriceView)).check(matches(isDisplayed()));
        onView(is(viewHolder.mTotalPriceLabelView)).check(matches(isDisplayed()));
        onView(is(viewHolder.mTotalPriceView)).check(matches(isDisplayed()));
        // When total price is set, descriptionLine3 is shown in mPriceAttributionView instead.
        onView(is(viewHolder.mPriceAttributionView)).check(matches(isDisplayed()));
        onView(is(viewHolder.mDescriptionLine3View)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testTitle() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        /* All description lines are set, title must be in single line. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        null, "line 1", "line 2", "line 3", false, false, false,
                                        false, false, false)));
        onView(is(viewHolder.mTitleView)).check(matches(isTextMaxLines(1)));

        /* Description line 2 not set, title may wrap once. */
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantDetailsModel.DETAILS,
                                              new AssistantDetails("title", "image", null, false,
                                                      "Total", "$12", null, "line 1", "", "line 3",
                                                      false, false, false, false, false, false)));
        onView(is(viewHolder.mTitleView)).check(matches(isTextMaxLines(2)));

        /* Description line 1 not set, title may wrap once.*/
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantDetailsModel.DETAILS,
                                              new AssistantDetails("title", "image", null, false,
                                                      "Total", "$12", null, "", "line 2", "line 3",
                                                      false, false, false, false, false, false)));
        onView(is(viewHolder.mTitleView)).check(matches(isTextMaxLines(2)));

        /* Description lines 1 and 2 not set, title may wrap twice.*/
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantDetailsModel.DETAILS,
                                              new AssistantDetails("title", "image", null, false,
                                                      "Total", "$12", null, "", "", "line 3", false,
                                                      false, false, false, false, false)));
        onView(is(viewHolder.mTitleView)).check(matches(isTextMaxLines(3)));

        /* Check title text and visibility. */
        onView(is(viewHolder.mTitleView)).check(matches(allOf(withText("title"), isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDescriptionLine1() throws Exception {
        Locale locale = LocaleUtils.forLanguageTag("en-US");
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model, locale);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        /* Both line 1 and date are set: description line 1 should show line 1. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        java.util.Calendar.getInstance().getTime(), "line 1",
                                        "line 2", "line 3", false, false, false, false, false,
                                        false)));
        onView(is(viewHolder.mDescriptionLine1View))
                .check(matches(allOf(withText("line 1"), isDisplayed())));

        /*
         * Line 1 is empty, but date is set: description line 1 should show the date & time.
         * Note: locale was set to en-US, so we know the output format for date and time.
         */
        Calendar calendar = java.util.Calendar.getInstance(locale);
        calendar.set(2050, 6, 25, 10, 05, 00);
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        calendar.getTime(), "", "line 2", "line 3", false, false,
                                        false, false, false, false)));
        onView(is(viewHolder.mDescriptionLine1View))
                .check(matches(withText(containsString("Mon, Jul 25"))));
        onView(is(viewHolder.mDescriptionLine1View))
                .check(matches(withText(containsString("10:05 AM"))));

        /* Line 1 is empty, date is not set: description line 1 should be invisible. */
        runOnUiThreadBlocking(()
                                      -> model.set(AssistantDetailsModel.DETAILS,
                                              new AssistantDetails("title", "image", null, false,
                                                      "Total", "$12", null, "", "line 2", "line 3",
                                                      false, false, false, false, false, false)));
        onView(is(viewHolder.mDescriptionLine1View)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDescriptionLine1NonUSLocale() throws Exception {
        Locale locale = LocaleUtils.forLanguageTag("de-DE");
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model, locale);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        Calendar calendar = java.util.Calendar.getInstance(locale);
        calendar.set(2050, 6, 25, 10, 05, 00);
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        calendar.getTime(), "", "line 2", "line 3", false, false,
                                        false, false, false, false)));
        onView(is(viewHolder.mDescriptionLine1View))
                .check(matches(withText(containsString("Mo., Juli 25"))));
        onView(is(viewHolder.mDescriptionLine1View))
                .check(matches(withText(containsString("10:05"))));
    }

    @Test
    @MediumTest
    public void testDescriptionLine2() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        /* Description line 2 is set and should be visible. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        java.util.Calendar.getInstance().getTime(), "line 1",
                                        "line 2", "line 3", false, false, false, false, false,
                                        false)));
        onView(is(viewHolder.mDescriptionLine2View))
                .check(matches(allOf(withText("line 2"), isDisplayed())));

        /* Description line 2 is not set and should be invisible. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        java.util.Calendar.getInstance().getTime(), "line 1", "",
                                        "line 3", false, false, false, false, false, false)));
        onView(is(viewHolder.mDescriptionLine2View)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDescriptionLine3() throws Exception {
        AssistantDetailsModel model = new AssistantDetailsModel();
        AssistantDetailsCoordinator coordinator = createCoordinator(model);
        ViewHolder viewHolder = runOnUiThreadBlocking(() -> new ViewHolder(coordinator.getView()));

        /* No total price set, line 3 should be displayed in the normal spot. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "", "",
                                        java.util.Calendar.getInstance().getTime(), "line 1",
                                        "line 2", "line 3", false, false, false, false, false,
                                        false)));
        onView(is(viewHolder.mDescriptionLine3View))
                .check(matches(allOf(withText("line 3"), isDisplayed())));
        onView(is(viewHolder.mPriceAttributionView)).check(matches(not(isDisplayed())));

        /* Total price and line 3 set, line 3 should be displayed in a different view. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        java.util.Calendar.getInstance().getTime(), "line 1", "",
                                        "line 3", false, false, false, false, false, false)));
        onView(is(viewHolder.mPriceAttributionView))
                .check(matches(allOf(withText("line 3"), isDisplayed())));
        onView(is(viewHolder.mDescriptionLine3View)).check(matches(not(isDisplayed())));

        /* Line 3 not set, both views should be invisible. */
        runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantDetailsModel.DETAILS,
                                new AssistantDetails("title", "image", null, false, "Total", "$12",
                                        java.util.Calendar.getInstance().getTime(), "line 1", "",
                                        "", false, false, false, false, false, false)));
        onView(is(viewHolder.mDescriptionLine3View)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mPriceAttributionView)).check(matches(not(isDisplayed())));
    }
}
