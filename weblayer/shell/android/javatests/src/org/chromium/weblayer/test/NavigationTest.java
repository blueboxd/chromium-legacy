// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NavigationObserver;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NavigationTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    // URLs used for base tests.
    private static final String URL1 = "data:text,foo";
    private static final String URL2 = "data:text,bar";
    private static final String URL3 = "data:text,baz";

    private static class Observer extends NavigationObserver {
        public static class NavigationCallbackHelper extends CallbackHelper {
            private Uri mUri;

            public void notifyCalled(Uri uri) {
                mUri = uri;
                notifyCalled();
            }

            public void assertCalledWith(int currentCallCount, String uri) throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
            }
        }

        public static class NavigationObserverValueRecorder {
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
                        new Criteria() {
                            @Override
                            public boolean isSatisfied() {
                                return mObservedValues.contains(expectation);
                            }
                        },
                        CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                        CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            }
        }

        public NavigationCallbackHelper onStartedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCommittedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCompletedCallback = new NavigationCallbackHelper();
        public NavigationObserverValueRecorder loadStateChangedCallback =
                new NavigationObserverValueRecorder();
        public NavigationObserverValueRecorder loadProgressChangedCallback =
                new NavigationObserverValueRecorder();
        public CallbackHelper onFirstContentfulPaintCallback = new CallbackHelper();

        @Override
        public void navigationStarted(Navigation navigation) {
            onStartedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void navigationCommitted(Navigation navigation) {
            onCommittedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void navigationCompleted(Navigation navigation) {
            onCompletedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void onFirstContentfulPaint() {
            onFirstContentfulPaintCallback.notifyCalled();
        }

        @Override
        public void loadStateChanged(boolean isLoading, boolean toDifferentDocument) {
            loadStateChangedCallback.recordValue(
                    Boolean.toString(isLoading) + " " + Boolean.toString(toDifferentDocument));
        }

        @Override
        public void loadProgressChanged(double progress) {
            loadProgressChangedCallback.recordValue(
                    progress == 1 ? "load complete" : "load started");
        }
    }

    private final Observer mObserver = new Observer();

    @Test
    @SmallTest
    public void testNavigationEvents() throws Exception {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(URL1);

        setNavigationObserver(activity);
        int curStartedCount = mObserver.onStartedCallback.getCallCount();
        int curCommittedCount = mObserver.onCommittedCallback.getCallCount();
        int curCompletedCount = mObserver.onCompletedCallback.getCallCount();
        int curOnFirstContentfulPaintCount =
                mObserver.onFirstContentfulPaintCallback.getCallCount();

        mActivityTestRule.navigateAndWait(URL2);

        mObserver.onStartedCallback.assertCalledWith(curStartedCount, URL2);
        mObserver.onCommittedCallback.assertCalledWith(curCommittedCount, URL2);
        mObserver.onCompletedCallback.assertCalledWith(curCompletedCount, URL2);
        mObserver.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
    }

    @Test
    @SmallTest
    public void testLoadStateUpdates() throws Exception {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationObserver(activity);
        mActivityTestRule.navigateAndWait(URL1);

        /* Wait until the NavigationObserver is notified of load completion. */
        mObserver.loadStateChangedCallback.waitUntilValueObserved("false false");
        mObserver.loadProgressChangedCallback.waitUntilValueObserved("load complete");

        /* Verify that the NavigationObserver was notified of load progress /before/ load
         * completion.
         */
        int finishStateIndex =
                mObserver.loadStateChangedCallback.getObservedValues().indexOf("false false");
        int finishProgressIndex =
                mObserver.loadProgressChangedCallback.getObservedValues().indexOf("load complete");
        int startStateIndex =
                mObserver.loadStateChangedCallback.getObservedValues().lastIndexOf("true true");
        int startProgressIndex =
                mObserver.loadProgressChangedCallback.getObservedValues().lastIndexOf(
                        "load started");

        assertNotEquals(startStateIndex, -1);
        assertNotEquals(startProgressIndex, -1);
        assertNotEquals(finishStateIndex, -1);
        assertNotEquals(finishProgressIndex, -1);

        assertTrue(startStateIndex < finishStateIndex);
        assertTrue(startProgressIndex < finishProgressIndex);
    }

    @Test
    @SmallTest
    public void testGoBackAndForward() throws Exception {
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationObserver(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController =
                    activity.getBrowserController().getNavigationController();

        navigateAndWaitForCompletion(URL2, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL1, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL2, () -> {
            assertFalse(navigationController.canGoBack());
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        navigateAndWaitForCompletion(URL3, () -> {
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        runOnUiThreadBlocking(() -> {
            assertFalse(navigationController.canGoForward());
        });
    }

    private void setNavigationObserver(WebLayerShellActivity activity) {
        runOnUiThreadBlocking(() ->
            activity.getBrowserController().getNavigationController().addObserver(mObserver)
        );
    }

    private void navigateAndWaitForCompletion(String expectedUrl, Runnable navigateRunnable)
            throws Exception {
        int currentCallCount = mObserver.onCompletedCallback.getCallCount();
        runOnUiThreadBlocking(navigateRunnable);
        mObserver.onCompletedCallback.assertCalledWith(currentCallCount, expectedUrl);
    }
}
