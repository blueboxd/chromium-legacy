// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighterTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests the app menu popup. Covers AppMenuCoordinatorImpl and public interface for
 * AppMenuHandlerImpl.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppMenuTest extends DummyUiActivityTestCase {
    private AppMenuCoordinatorImpl mAppMenuCoordinator;
    private AppMenuHandlerImpl mAppMenuHandler;
    private TestAppMenuPropertiesDelegate mPropertiesDelegate;
    private TestAppMenuDelegate mDelegate;
    private TestAppMenuObserver mMenuObserver;
    private TestActivityLifecycleDispatcher mLifecycleDispatcher;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(this::setUpTestOnUiThread);
        mLifecycleDispatcher.observerRegisteredCallbackHelper.waitForCallback(0);
    }

    private void setUpTestOnUiThread() {
        mLifecycleDispatcher = new TestActivityLifecycleDispatcher();
        mDelegate = new TestAppMenuDelegate();
        mAppMenuCoordinator = new AppMenuCoordinatorImpl(getActivity(), mLifecycleDispatcher,
                new TestMenuButtonDelegate(), mDelegate, getActivity().getWindow().getDecorView(),
                new View(getActivity()));
        mAppMenuHandler = mAppMenuCoordinator.getAppMenuHandlerImplForTesting();
        mMenuObserver = new TestAppMenuObserver();
        mAppMenuCoordinator.getAppMenuHandler().addObserver(mMenuObserver);
        mPropertiesDelegate =
                (TestAppMenuPropertiesDelegate) mAppMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    @Test
    @MediumTest
    public void testShowHideAppMenu() throws TimeoutException {
        showSimpleMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.hideAppMenu());
        mMenuObserver.menuHiddenCallback.waitForCallback(0);

        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after hide", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuCoordinator.destroy());
        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after destroy", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testShowDestroyAppMenu() throws TimeoutException {
        showSimpleMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuCoordinator.destroy());

        Assert.assertEquals("Incorrect number of calls to #onMenuDismissed after destroy", 1,
                mPropertiesDelegate.menuDismissedCallback.getCallCount());
    }

    @Test
    @MediumTest
    public void testClickMenuItem() throws TimeoutException {
        showSimpleMenuAndAssert();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AppMenuTestSupport.callOnItemClick(
                                mAppMenuCoordinator, R.id.menu_item_three));
        mDelegate.itemSelectedCallbackHelper.waitForCallback(0);

        Assert.assertEquals("Incorrect id for last selected item.", R.id.menu_item_three,
                mDelegate.lastSelectedItemId);
    }

    @Test
    @MediumTest
    public void testAppMenuBlockers() throws TimeoutException {
        Assert.assertTrue("App menu should be allowed to show, no blockers registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));

        AppMenuBlocker blocker1 = () -> false;
        AppMenuBlocker blocker2 = () -> true;

        mAppMenuCoordinator.registerAppMenuBlocker(blocker1);
        mAppMenuCoordinator.registerAppMenuBlocker(blocker2);
        Assert.assertFalse("App menu should not be allowed to show, both blockers registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuCoordinator.showAppMenuForKeyboardEvent());
        Assert.assertFalse(
                "App menu should not have been shown.", mAppMenuHandler.isAppMenuShowing());

        mAppMenuCoordinator.unregisterAppMenuBlocker(blocker1);
        Assert.assertTrue("App menu should be allowed to show, only blocker2 registered",
                AppMenuTestSupport.shouldShowAppMenu(mAppMenuCoordinator));
        showSimpleMenuAndAssert();
    }

    @Test
    @MediumTest
    public void testSetMenuHighlight_StandardItem() throws TimeoutException {
        Assert.assertFalse(mMenuObserver.menuHighlighting);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.setMenuHighlight(R.id.menu_item_one, false));
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(0);
        Assert.assertTrue(mMenuObserver.menuHighlighting);

        showSimpleMenuAndAssert();

        View itemView = getViewAtPosition(0);
        ViewHighlighterTestUtils.checkHighlightOn(itemView);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.clearMenuHighlight());
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(1);
        Assert.assertFalse(mMenuObserver.menuHighlighting);
        ViewHighlighterTestUtils.checkHighlightOff(itemView);
    }

    @Test
    @MediumTest
    public void testSetMenuHighlight_Icon() throws TimeoutException {
        mPropertiesDelegate.enableAppIconRow = true;

        Assert.assertFalse(mMenuObserver.menuHighlighting);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.setMenuHighlight(R.id.icon_one, false));
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(0);
        Assert.assertTrue(mMenuObserver.menuHighlighting);

        showSimpleMenuAndAssert();

        View itemView = ((LinearLayout) getViewAtPosition(3)).getChildAt(0);
        ViewHighlighterTestUtils.checkHighlightOn(itemView);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.clearMenuHighlight());
        mMenuObserver.menuHighlightChangedCallback.waitForCallback(1);
        Assert.assertFalse(mMenuObserver.menuHighlighting);
        ViewHighlighterTestUtils.checkHighlightOff(itemView);
    }

    @Test
    @MediumTest
    public void testMenuItemContentChanged() throws TimeoutException {
        showSimpleMenuAndAssert();
        View itemView = getViewAtPosition(1);
        Assert.assertEquals("Menu item text incorrect", "Menu Item Two",
                ((TextView) itemView.findViewById(R.id.menu_item_text)).getText());

        String newText = "Test!";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAppMenuHandler.getAppMenu().getMenu().findItem(R.id.menu_item_two).setTitle(newText);
            mAppMenuHandler.menuItemContentChanged(R.id.menu_item_two);
        });

        itemView = getViewAtPosition(1);
        Assert.assertEquals("Menu item text incorrect", newText,
                ((TextView) itemView.findViewById(R.id.menu_item_text)).getText());
    }

    @Test
    @MediumTest
    public void testHeaderFooter() throws TimeoutException {
        mPropertiesDelegate.headerResourceId = R.layout.test_menu_header;
        mPropertiesDelegate.footerResourceId = R.layout.test_menu_footer;
        showSimpleMenuAndAssert();

        mPropertiesDelegate.headerInflatedCallback.waitForCallback(0);
        mPropertiesDelegate.footerInflatedCallback.waitForCallback(0);

        Assert.assertEquals("Incorrect number of header views", 1,
                mAppMenuHandler.getAppMenu().getListView().getHeaderViewsCount());
        Assert.assertNotNull("Footer stub not inflated.",
                mAppMenuHandler.getAppMenu().getPopup().getContentView().findViewById(
                        R.id.app_menu_footer));
    }

    @Test
    @MediumTest
    public void testAppMenuHiddenOnStopWithNative() throws TimeoutException {
        showSimpleMenuAndAssert();
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.onStopWithNative());
        Assert.assertFalse(mAppMenuHandler.isAppMenuShowing());
    }

    @Test
    @MediumTest
    public void testAppMenuHiddenOnConfigurationChange() throws TimeoutException {
        showSimpleMenuAndAssert();
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppMenuHandler.onConfigurationChanged(null));
        Assert.assertFalse(mAppMenuHandler.isAppMenuShowing());
    }

    private void showSimpleMenuAndAssert() throws TimeoutException {
        int currentCallCount = mMenuObserver.menuShownCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuCoordinator.showAppMenuForKeyboardEvent());
        mMenuObserver.menuShownCallback.waitForCallback(currentCallCount);
        Assert.assertTrue("Menu should be showing", mAppMenuHandler.isAppMenuShowing());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAppMenuHandler.getAppMenu().finishAnimationsForTests());
    }

    private class TestActivityLifecycleDispatcher implements ActivityLifecycleDispatcher {
        public CallbackHelper observerRegisteredCallbackHelper = new CallbackHelper();
        @Override
        public void register(LifecycleObserver observer) {
            observerRegisteredCallbackHelper.notifyCalled();
        }

        @Override
        public void unregister(LifecycleObserver observer) {}

        @Override
        public int getCurrentActivityState() {
            return 0;
        }
    }

    private class TestMenuButtonDelegate implements MenuButtonDelegate {
        @Nullable
        @Override
        public View getMenuButtonView() {
            return null;
        }

        @Override
        public boolean isMenuFromBottom() {
            return false;
        }
    }

    private View getViewAtPosition(int index) {
        // Wait for the view to be available. This is necessary when the menu is first shown.
        CriteriaHelper.pollUiThread(
                ()
                        -> AppMenuTestSupport.getListView(mAppMenuCoordinator).getChildAt(index)
                        != null);
        return AppMenuTestSupport.getListView(mAppMenuCoordinator).getChildAt(index);
    }
}
