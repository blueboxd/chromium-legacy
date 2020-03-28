// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * End-to-end test for TabSelectionEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
public class TabSelectionEditorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    private TabSelectionEditorTestingRobot mRobot = new TabSelectionEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private TabSelectionEditorLayout mTabSelectionEditorLayout;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        prepareBlankTab(2, false);

        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabSelectionEditorCoordinator tabSelectionEditorCoordinator =
                    new TabSelectionEditorCoordinator(mActivityTestRule.getActivity(),
                            (ViewGroup) mActivityTestRule.getActivity().getWindow().getDecorView(),
                            mTabModelSelector,
                            mActivityTestRule.getActivity().getTabContentManager(), null,
                            getMode());

            mTabSelectionEditorController = tabSelectionEditorCoordinator.getController();
            mTabSelectionEditorLayout =
                    tabSelectionEditorCoordinator.getTabSelectionEditorLayoutForTesting();
        });
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()
                        && SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), isIncognito, true);
            mActivityTestRule.loadUrl("about:blank");
        }
    }

    @Test
    @MediumTest
    public void testShowTabs() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(1);
    }

    @Test
    @MediumTest
    public void testToggleItem() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0).verifyToolbarSelectionText(
                "1 selected");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyItemNotSelectedAtAdapterPosition(0)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs);
    }

    @Test
    @MediumTest
    public void testToolbarNavigationButtonHideTabSelectionEditor() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    public void testToolbarGroupButtonEnabledState() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionButtonEnabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();
    }

    @Test
    @MediumTest
    public void testToolbarGroupButton() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.resultRobot.verifyToolbarActionButtonWithResourceId(
                R.string.tab_selection_editor_group);

        mRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionButton();

        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();

        // TODO(1021803): verify the undo snack after the bug is resolved.
        // verifyUndoSnackbarWithTextIsShown(mActivityTestRule.getActivity().getString(
        //     R.string.undo_bar_group_tabs_message, 2));
    }

    @Test
    @MediumTest
    public void testConfigureToolbar_ActionButtonEnableThreshold() {
        List<Tab> tabs = getTabsInCurrentTabModel();

        int enableThreshold = 1;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabSelectionEditorController.configureToolbar("Test", null, enableThreshold, null);
            mTabSelectionEditorController.show(tabs);
        });

        mRobot.resultRobot.verifyToolbarActionButtonDisabled().verifyToolbarActionButtonWithText(
                "Test");

        for (int i = 0; i < enableThreshold; i++) {
            mRobot.actionRobot.clickItemAtAdapterPosition(i);
        }
        mRobot.resultRobot.verifyToolbarActionButtonEnabled();

        mRobot.actionRobot.clickItemAtAdapterPosition(enableThreshold - 1);
        mRobot.resultRobot.verifyToolbarActionButtonDisabled();
    }

    @Test
    @MediumTest
    public void testShowTabsWithPreSelectedTabs() {
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group)
                .verifyToolbarSelectionText("1 selected")
                .verifyHasAtLeastNItemVisible(tabs.size() + 1)
                .verifyItemSelectedAtAdapterPosition(0)
                .verifyHasItemViewTypeAtAdapterPosition(1, TabProperties.UiType.DIVIDER)
                .verifyDividerAlwaysStartsAtTheEdgeOfScreen();
    }

    @Test
    @MediumTest
    public void testShowTabsWithPreSelectedTabs_10Tabs() {
        int preSelectedTabCount = 10;
        int additionalTabCount =
                preSelectedTabCount + 1 - mTabModelSelector.getCurrentModel().getCount();

        for (int i = 0; i < additionalTabCount; i++) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), mTabModelSelector.isIncognitoSelected(), true);
        }

        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyToolbarSelectionText("10 selected")
                .verifyHasItemViewTypeAtAdapterPosition(
                        preSelectedTabCount, TabProperties.UiType.DIVIDER)
                .verifyDividerAlwaysStartsAtTheEdgeOfScreenAtPosition(preSelectedTabCount);
    }

    @Test
    @MediumTest
    public void testDividerIsNotClickable() {
        List<Tab> tabs = getTabsInCurrentTabModel();
        int preSelectedTabCount = 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTabSelectionEditorController.show(tabs, preSelectedTabCount));

        mRobot.resultRobot.verifyDividerNotClickableNotFocusable();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testListViewAppearance() throws IOException {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testListViewAppearance_oneSelectedTab() throws IOException {
        List<Tab> tabs = getTabsInCurrentTabModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mTabSelectionEditorController.show(tabs));

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        mRenderTestRule.render(mTabSelectionEditorLayout, "list_view_one_selected_tab");
    }

    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }

        return tabs;
    }
}
