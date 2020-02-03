// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.PositionAssertions.isLeftAlignedWith;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.iterableWithSize;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BooleanList;
import org.chromium.chrome.browser.autofill_assistant.proto.CallbackProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientDimensionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataResultProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ColorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DividerViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.EventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GenericUserInterfaceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ImageViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.LinearLayoutProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ModelProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnModelValueChangedEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnViewClickedEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetModelValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShapeDrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowInfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StringList;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueProto;
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
                        "TextAppearance.TextMedium.Secondary"))
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
    @DisabledTest(message = "crbug.com/1033877")
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testStaticUserInterface() {
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

        ViewProto locationImage = createTestImage("btn_close", "locationImage");
        ViewProto locationTextView =
                createTextView("345 Spear Street, San Francisco", "locationText");
        ViewProto locationChevron = createTestImage("ic_expand_more_black_24dp", "locationChevron");
        ViewProto locationSection = createSectionView(
                Arrays.asList(locationImage, locationTextView, locationChevron), "locationSection");

        ViewProto cardImage = createTestImage("btn_close", "cardImage");
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
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder().setRootView(
                                                         rootView))
                                         .setPrivacyNoticeText(
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

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testOnViewClickedWriteToModel() {
        ViewProto clickableView1 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'true' to output_1 when clicked"))
                                           .setIdentifier("clickableView1")
                                           .build();
        ViewProto clickableView2 = (ViewProto) ViewProto.newBuilder()
                                           .setTextView(TextViewProto.newBuilder().setText(
                                                   "Writes 'Hello World' to output_2 when clicked"))
                                           .setIdentifier("clickableView2")
                                           .build();

        ViewProto rootViewPrepended =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView1))
                        .build();
        ViewProto rootViewAppended =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView2))
                        .build();

        List<InteractionProto> interactionsPrepended = new ArrayList<>();
        interactionsPrepended.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .setTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder()
                                        .setViewIdentifier("clickableView1")
                                        .setValue(ValueProto.newBuilder().setBooleans(
                                                BooleanList.newBuilder().addValues(true)))))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_1")))
                        .build());
        List<InteractionProto> interactionsAppended = new ArrayList<>();
        interactionsAppended.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .setTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder()
                                        .setViewIdentifier("clickableView2")
                                        .setValue(ValueProto.newBuilder().setStrings(
                                                StringList.newBuilder().addValues("Hello World")))))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_2")))
                        .build());

        List<ModelProto.ModelValue> modelValuesPrepended = new ArrayList<>();
        modelValuesPrepended.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                         .setIdentifier("output_1")
                                         .build());
        List<ModelProto.ModelValue> modelValuesAppended = new ArrayList<>();
        modelValuesAppended.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                        .setIdentifier("output_2")
                                        .build());

        GenericUserInterfaceProto genericUserInterfacePrepended =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootViewPrepended)
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                interactionsPrepended))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesPrepended))
                        .build();

        GenericUserInterfaceProto genericUserInterfaceAppended =
                (GenericUserInterfaceProto) GenericUserInterfaceProto.newBuilder()
                        .setRootView(rootViewAppended)
                        .setInteractions(InteractionsProto.newBuilder().addAllInteractions(
                                interactionsAppended))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValuesAppended))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 genericUserInterfacePrepended)
                                         .setGenericUserInterfaceAppended(
                                                 genericUserInterfaceAppended)
                                         .setPrivacyNoticeText(
                                                 "Chrome will send selected data to example.com")
                                         .setRequestTermsAndConditions(false))
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

        onView(withText("Writes 'true' to output_1 when clicked")).perform(click());
        onView(withText("Writes 'Hello World' to output_2 when clicked")).perform(click());

        // Finish action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                (ActionProto) ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(2));
        assertThat(resultModelValues,
                containsInAnyOrder((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                           .setIdentifier("output_1")
                                           .setValue(ValueProto.newBuilder().setBooleans(
                                                   BooleanList.newBuilder().addValues(true)))
                                           .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build()));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testCallbackChain() {
        ViewProto clickableView =
                (ViewProto) ViewProto.newBuilder()
                        .setTextView(TextViewProto.newBuilder().setText(
                                "Writes 'Hello World' to output_1 and output_3 when clicked"))
                        .setIdentifier("clickableView")
                        .build();

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .setTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                OnViewClickedEventProto.newBuilder()
                                        .setViewIdentifier("clickableView")
                                        .setValue(ValueProto.newBuilder().setStrings(
                                                StringList.newBuilder().addValues("Hello World")))))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_1")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_3")))
                        .build());
        // Whenever output_1 changes, copy the value to output_2.
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .setTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output_1")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_2")))
                        .build());
        // Whenever output_2 changes, copy the value to output_1. This tests that no infinite loop
        // is created, because events should only be fired for actual value changes.
        interactions.add(
                (InteractionProto) InteractionProto.newBuilder()
                        .setTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "output_2")))
                        .addCallbacks(CallbackProto.newBuilder().setSetValue(
                                SetModelValueProto.newBuilder().setModelIdentifier("output_1")))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_1")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .build());
        modelValues.add((ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_3")
                                .build());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder()
                                                         .setRootView(rootView)
                                                         .setInteractions(
                                                                 InteractionsProto.newBuilder()
                                                                         .addAllInteractions(
                                                                                 interactions))
                                                         .setModel(ModelProto.newBuilder()
                                                                           .addAllValues(
                                                                                   modelValues)))
                                         .setPrivacyNoticeText(
                                                 "Chrome will send selected data to example.com")
                                         .setRequestTermsAndConditions(false))
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

        onView(withText("Writes 'Hello World' to output_1 and output_3 when clicked"))
                .perform(click());

        // Finish action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                (ActionProto) ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        List<ModelProto.ModelValue> resultModelValues = result.getModel().getValuesList();
        assertThat(resultModelValues, iterableWithSize(3));
        assertThat(resultModelValues,
                containsInAnyOrder(
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_1")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_2")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build(),
                        (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                                .setIdentifier("output_3")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        StringList.newBuilder().addValues("Hello World")))
                                .build()));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testShowInfoPopupOnClick() {
        ViewProto clickableView = (ViewProto) ViewProto.newBuilder()
                                          .setTextView(TextViewProto.newBuilder().setText(
                                                  "Shows an info popup when clicked"))
                                          .setIdentifier("clickableView")
                                          .build();

        ViewProto rootView =
                (ViewProto) ViewProto.newBuilder()
                        .setViewContainer(
                                ViewContainerProto.newBuilder()
                                        .setLinearLayout(
                                                LinearLayoutProto.newBuilder().setOrientation(
                                                        LinearLayoutProto.Orientation.VERTICAL))
                                        .addViews(clickableView))
                        .build();

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add((InteractionProto) InteractionProto.newBuilder()
                                 .setTriggerEvent(EventProto.newBuilder().setOnViewClicked(
                                         OnViewClickedEventProto.newBuilder().setViewIdentifier(
                                                 "clickableView")))
                                 .addCallbacks(CallbackProto.newBuilder().setShowInfoPopup(
                                         ShowInfoPopupProto.newBuilder().setInfoPopup(
                                                 InfoPopupProto.newBuilder()
                                                         .setText("Info message here")
                                                         .setTitle("Some title"))))
                                 .build());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setGenericUserInterfacePrepended(
                                                 GenericUserInterfaceProto.newBuilder()
                                                         .setRootView(rootView)
                                                         .setInteractions(
                                                                 InteractionsProto.newBuilder()
                                                                         .addAllInteractions(
                                                                                 interactions)))
                                         .setPrivacyNoticeText(
                                                 "Chrome will send selected data to example.com")
                                         .setRequestTermsAndConditions(false))
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

        onView(withText("Shows an info popup when clicked")).perform(click());
        onView(withText("Some title")).check(matches(isDisplayed()));
        onView(withText("Info message here")).check(matches(isDisplayed()));
        onView(withText(mTestRule.getActivity().getString(R.string.close))).perform(click());
    }
}
