// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests that BrowserObserver methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class BrowserObserverTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    // URL used for base tests.
    private static final String URL = "data:text";

    private static class Observer extends BrowserObserver {
        public static class BrowserObserverValueRecorder {
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

        public BrowserObserverValueRecorder visibleUrlChangedCallback =
                new BrowserObserverValueRecorder();
        public BrowserObserverValueRecorder loadingStateChangedCallback =
                new BrowserObserverValueRecorder();
        public BrowserObserverValueRecorder loadProgressChangedCallback =
                new BrowserObserverValueRecorder();

        @Override
        public void visibleUrlChanged(Uri url) {
            visibleUrlChangedCallback.recordValue(url.toString());
        }

        @Override
        public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {
            loadingStateChangedCallback.recordValue(
                    Boolean.toString(isLoading) + " " + Boolean.toString(toDifferentDocument));
        }

        @Override
        public void loadProgressChanged(double progress) {
            loadProgressChangedCallback.recordValue(
                    progress == 1 ? "load complete" : "load started");
        }
    }

    @Test
    @SmallTest
    public void testLoadEvents() {
        String startupUrl = "about:blank";
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Observer observer = new Observer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowserController().addObserver(observer); });

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        /* Verify that the visible URL changes to the target. */
        observer.visibleUrlChangedCallback.waitUntilValueObserved(url);

        /* Wait until the BrowserObserver is notified of load completion. */
        observer.loadingStateChangedCallback.waitUntilValueObserved("false true");
        observer.loadProgressChangedCallback.waitUntilValueObserved("load complete");

        /* Verify that the BrowserObserver was notified of load progress /before/ load completion.
         */
        int finishStateIndex =
                observer.loadingStateChangedCallback.getObservedValues().indexOf("false true");
        int finishProgressIndex =
                observer.loadProgressChangedCallback.getObservedValues().indexOf("load complete");
        int startStateIndex =
                observer.loadingStateChangedCallback.getObservedValues().lastIndexOf("true true");
        int startProgressIndex =
                observer.loadProgressChangedCallback.getObservedValues().lastIndexOf(
                        "load started");

        Assert.assertNotEquals(startStateIndex, -1);
        Assert.assertNotEquals(startProgressIndex, -1);
        Assert.assertNotEquals(finishStateIndex, -1);
        Assert.assertNotEquals(finishProgressIndex, -1);

        Assert.assertTrue(startStateIndex < finishStateIndex);
        Assert.assertTrue(startProgressIndex < finishProgressIndex);
    }
}
