// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.weblayer.ContextMenuParams;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests that TabCallback methods are invoked as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static class Callback extends TabCallback {
        public static class TabCallbackValueRecorder {
            private List<String> mObservedValues =
                    Collections.synchronizedList(new ArrayList<String>());

            public void recordValue(String parameter) {
                mObservedValues.add(parameter);
            }

            public List<String> getObservedValues() {
                return mObservedValues;
            }

            public void waitUntilValueObserved(String expectation) {
                CriteriaHelper.pollInstrumentationThread(
                        () -> Assert.assertThat(expectation, Matchers.isIn(mObservedValues)));
            }
        }

        public TabCallbackValueRecorder visibleUriChangedCallback = new TabCallbackValueRecorder();

        @Override
        public void onVisibleUriChanged(Uri uri) {
            visibleUriChangedCallback.recordValue(uri.toString());
        }
    }

    @Test
    @SmallTest
    public void testLoadEvents() {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Callback callback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getTab().registerTabCallback(callback); });

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        /* Verify that the visible URL changes to the target. */
        callback.visibleUriChangedCallback.waitUntilValueObserved(url);
    }

    @Test
    @SmallTest
    public void testOnRenderProcessGone() throws TimeoutException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onRenderProcessGone() {
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
            tab.getNavigationController().navigate(Uri.parse("chrome://crash"));
        });
        callbackHelper.waitForFirst();
    }

    // Requires implementation M82.
    @Test
    @SmallTest
    public void testShowContextMenu() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("download.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);

        ContextMenuParams params[] = new ContextMenuParams[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void showContextMenu(ContextMenuParams param) {
                    params[0] = param;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        TestTouchUtils.longClickView(
                InstrumentationRegistry.getInstrumentation(), activity.getWindow().getDecorView());
        callbackHelper.waitForFirst();
        Assert.assertEquals(Uri.parse(pageUrl), params[0].pageUri);
        Assert.assertEquals(
                Uri.parse(mActivityTestRule.getTestDataURL("lorem_ipsum.txt")), params[0].linkUri);
        Assert.assertEquals("anchor text", params[0].linkText);
    }

    // Requires implementation M82.
    @Test
    @SmallTest
    public void testShowContextMenuImg() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("img.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);

        ContextMenuParams params[] = new ContextMenuParams[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void showContextMenu(ContextMenuParams param) {
                    params[0] = param;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        TestTouchUtils.longClickView(
                InstrumentationRegistry.getInstrumentation(), activity.getWindow().getDecorView());
        callbackHelper.waitForFirst();
        Assert.assertEquals(Uri.parse(pageUrl), params[0].pageUri);
        Assert.assertEquals(
                Uri.parse(mActivityTestRule.getTestDataURL("notfound.png")), params[0].srcUri);
        Assert.assertEquals("alt_text", params[0].titleOrAltText);
    }

    @Test
    @SmallTest
    public void testTabModalOverlay() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("alert.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);
        Assert.assertNotNull(activity);

        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        int callCount = callbackHelper.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        callCount = callbackHelper.getCallCount();
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);
    }

    @Test
    @SmallTest
    public void testDismissTransientUi() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("alert.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);
        Assert.assertNotNull(activity);

        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        int callCount = callbackHelper.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        callCount = callbackHelper.getCallCount();
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);

        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
    }

    @Test
    @SmallTest
    public void testTabModalOverlayOnBackgroundTab() throws TimeoutException {
        // Create a tab.
        String url = mActivityTestRule.getTestDataURL("new_browser.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(activity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = activity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        // Tapping it creates a second tab, which is active.
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callback.waitForNewTab();

        Tab secondTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Assert.assertEquals(2, activity.getBrowser().getTabs().size());
            return activity.getBrowser().getActiveTab();
        });
        Assert.assertNotSame(firstTab, secondTab);

        // Track tab modal updates.
        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            firstTab.registerTabCallback(new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            });
        });

        // Create an alert from the background tab. It shouldn't display. There's no way to
        // consistently verify that nothing happens, but the script execution should finish, which
        // is not the case for dialogs that show on an active tab until they're dismissed.
        mActivityTestRule.executeScriptSync("window.alert('foo');", true, firstTab);
        Assert.assertEquals(0, callbackHelper.getCallCount());

        // When that tab becomes active again, the alert should show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setActiveTab(firstTab); });
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        // Switch away from the tab again; the alert should be hidden.
        callCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setActiveTab(secondTab); });
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);
    }

    @Test
    @SmallTest
    public void testOnTitleUpdated() throws TimeoutException {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        String titles[] = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTitleUpdated(String title) {
                    titles[0] = title;
                }
            };
            tab.registerTabCallback(callback);
        });

        String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(url);
        // Use polling because title is allowed to go through multiple transitions.
        CriteriaHelper.pollUiThread(() -> { return "OK".equals(titles[0]); });

        url = mActivityTestRule.getTestDataURL("shakespeare.html");
        mActivityTestRule.navigateAndWait(url);
        CriteriaHelper.pollUiThread(() -> { return titles[0].endsWith("shakespeare.html"); });

        mActivityTestRule.executeScriptSync("document.title = \"foobar\";", false);
        Assert.assertEquals("foobar", titles[0]);
        CriteriaHelper.pollUiThread(() -> { return "foobar".equals(titles[0]); });
    }
}
