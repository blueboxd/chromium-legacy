// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.PositionAssertions.isLeftAlignedWith;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientDimensionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ColorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DividerViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GenericUserInterfaceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ImageViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.LinearLayoutProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShapeDrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewAttributesProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewContainerProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewLayoutParamsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantGenericUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    private ViewProto createTestImage(String resourceId, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setImageView(ImageViewProto.newBuilder().setImage(
                        DrawableProto.newBuilder().setResourceIdentifier(resourceId)))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(24)
                                .setLayoutHeight(24)
                                .setLayoutGravity(ViewLayoutParamsProto.Gravity.CENTER.getNumber()))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createTextView(String text, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setTextView(TextViewProto.newBuilder().setText(text).setTextAppearance(
                        "TextAppearance.BlackBody"))
                .setAttributes(ViewAttributesProto.newBuilder().setPaddingStart(24))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(0)
                                .setLayoutWeight(1.0f)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createSectionView(List<ViewProto> views, String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setViewContainer(
                        ViewContainerProto.newBuilder()
                                .setLinearLayout(LinearLayoutProto.newBuilder().setOrientation(
                                        LinearLayoutProto.Orientation.HORIZONTAL))
                                .addAllViews(views))
                .setAttributes(ViewAttributesProto.newBuilder()
                                       .setPaddingStart(24)
                                       .setPaddingTop(24)
                                       .setPaddingEnd(24)
                                       .setPaddingBottom(24))
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setLayoutWidth(ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    private ViewProto createSectionDividerView(String identifier) {
        return (ViewProto) ViewProto.newBuilder()
                .setDividerView(DividerViewProto.newBuilder())
                .setLayoutParams(
                        ViewLayoutParamsProto.newBuilder()
                                .setMarginStart(0)
                                .setMarginEnd(0)
                                .setLayoutWidth(ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                .setLayoutHeight(ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                .setIdentifier(identifier)
                .build();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testRoundedRectangle() {
        DrawableProto roundedRect =
                (DrawableProto) DrawableProto.newBuilder()
                        .setShape(
                                ShapeDrawableProto.newBuilder()
                                        .setRectangle(ShapeDrawableProto.Rectangle.newBuilder()
                                                              .setCornerRadius(ClientDimensionProto
                                                                                       .newBuilder()
                                                                                       .setDp(16)))
                                        .setStrokeColor(ColorProto.newBuilder().setParseableColor(
                                                "#DADCE0"))
                                        .setStrokeWidth(ClientDimensionProto.newBuilder().setDp(1)))
                        .build();

        ViewProto locationImage = createTestImage("ic_place_googblue_36dp", "locationImage");
        ViewProto locationTextView =
                createTextView("345 Spear Street, San Francisco", "locationText");
        ViewProto locationChevron = createTestImage("ic_expand_more_black_24dp", "locationChevron");
        ViewProto locationSection = createSectionView(
                Arrays.asList(locationImage, locationTextView, locationChevron), "locationSection");

        ViewProto cardImage = createTestImage("ic_folder_blue_24dp", "cardImage");
        ViewProto cardTextView = createTextView("Visa •••• 1111", "cardText");
        ViewProto cardChevron = createTestImage("ic_expand_more_black_24dp", "cardChevron");
        ViewProto cardSection = createSectionView(
                Arrays.asList(cardImage, cardTextView, cardChevron), "cardSection");

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setAttributes(ViewAttributesProto.newBuilder().setBackground(roundedRect))
                        .setLayoutParams(
                                ViewLayoutParamsProto.newBuilder()
                                        .setMarginStart(24)
                                        .setMarginEnd(24)
                                        .setLayoutWidth(
                                                ViewLayoutParamsProto.Size.MATCH_PARENT_VALUE)
                                        .setLayoutHeight(
                                                ViewLayoutParamsProto.Size.WRAP_CONTENT_VALUE))
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(locationSection)
                                        .addViews(createSectionDividerView("divider"))
                                        .addViews(cardSection))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterface(
                                                 GenericUserInterfaceProto.newBuilder().setRootView(
                                                         rootView))
                                         .setThirdpartyPrivacyNoticeText(
                                                 "Chrome will send selected data to example.com")
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("End").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("345 Spear Street, San Francisco")).check(matches(isDisplayed()));
        onView(withText("Visa •••• 1111")).check(matches(isDisplayed()));

        onView(withText("345 Spear Street, San Francisco"))
                .check(isLeftAlignedWith(withText("Visa •••• 1111")));
        onView(withTagValue(is("locationChevron")))
                .check(isLeftAlignedWith(withTagValue(is("cardChevron"))));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
    }
}
