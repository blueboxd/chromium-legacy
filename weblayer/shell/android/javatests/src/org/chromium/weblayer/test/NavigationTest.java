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
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.shell.InstrumentationActivity;

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
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // URLs used for base tests.
    private static final String URL1 = "data:text,foo";
    private static final String URL2 = "data:text,bar";
    private static final String URL3 = "data:text,baz";

    private static class Callback extends NavigationCallback {
        public static class NavigationCallbackHelper extends CallbackHelper {
            private Uri mUri;
            private boolean mIsSameDocument;

            public void notifyCalled(Uri uri) {
                mUri = uri;
                notifyCalled();
            }

            public void notifyCalled(Uri uri, boolean isSameDocument) {
                mUri = uri;
                mIsSameDocument = isSameDocument;
                notifyCalled();
            }

            public void assertCalledWith(int currentCallCount, String uri) throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
            }

            public void assertCalledWith(int currentCallCount, String uri, boolean isSameDocument)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
                assertEquals(mIsSameDocument, isSameDocument);
            }
        }

        public static class NavigationCallbackValueRecorder {
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
        public NavigationCallbackHelper onReadyToCommitCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCompletedCallback = new NavigationCallbackHelper();
        public NavigationCallbackValueRecorder loadStateChangedCallback =
                new NavigationCallbackValueRecorder();
        public NavigationCallbackValueRecorder loadProgressChangedCallback =
                new NavigationCallbackValueRecorder();
        public CallbackHelper onFirstContentfulPaintCallback = new CallbackHelper();

        @Override
        public void navigationStarted(Navigation navigation) {
            onStartedCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void readyToCommitNavigation(Navigation navigation) {
            onReadyToCommitCallback.notifyCalled(navigation.getUri());
        }

        @Override
        public void navigationCompleted(Navigation navigation) {
            onCompletedCallback.notifyCalled(navigation.getUri(), navigation.isSameDocument());
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

    private final Callback mCallback = new Callback();

    @Test
    @SmallTest
    public void testNavigationEvents() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);

        setNavigationCallback(activity);
        int curStartedCount = mCallback.onStartedCallback.getCallCount();
        int curCommittedCount = mCallback.onReadyToCommitCallback.getCallCount();
        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();
        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();

        mActivityTestRule.navigateAndWait(URL2);

        mCallback.onStartedCallback.assertCalledWith(curStartedCount, URL2);
        mCallback.onReadyToCommitCallback.assertCalledWith(curCommittedCount, URL2);
        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, URL2);
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
    }

    @Test
    @SmallTest
    public void testLoadStateUpdates() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        mActivityTestRule.navigateAndWait(URL1);

        /* Wait until the NavigationCallback is notified of load completion. */
        mCallback.loadStateChangedCallback.waitUntilValueObserved("false false");
        mCallback.loadProgressChangedCallback.waitUntilValueObserved("load complete");

        /* Verify that the NavigationCallback was notified of load progress /before/ load
         * completion.
         */
        int finishStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().indexOf("false false");
        int finishProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().indexOf("load complete");
        int startStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().lastIndexOf("true true");
        int startProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().lastIndexOf(
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
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController = runOnUiThreadBlocking(
                () -> activity.getBrowserController().getNavigationController());

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

        runOnUiThreadBlocking(() -> { assertFalse(navigationController.canGoForward()); });
    }

    @Test
    @SmallTest
    public void testSameDocument() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        mActivityTestRule.executeScriptSync("history.pushState(null, '', '#bar');");

        mCallback.onCompletedCallback.assertCalledWith(
                curCompletedCount, "data:text,foo#bar", true);
    }

    private void setNavigationCallback(InstrumentationActivity activity) {
        runOnUiThreadBlocking(()
                                      -> activity.getBrowserController()
                                                 .getNavigationController()
                                                 .registerNavigationCallback(mCallback));
    }

    private void navigateAndWaitForCompletion(String expectedUrl, Runnable navigateRunnable)
            throws Exception {
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        runOnUiThreadBlocking(navigateRunnable);
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, expectedUrl);
    }
}
