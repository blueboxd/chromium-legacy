// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Looper;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.TimeoutTimer;
import org.chromium.content_public.browser.test.NestedSystemMessageHandler;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Utilities for use in Native Java Unit Tests.
 */
public class UnitTestUtils {
    /**
     * Polls the UI thread waiting for the Callable |criteria| to return true.
     *
     * In practice, this nests the looper and checks the criteria after each task is run as these
     * tests run on the UI thread and sleeping would block the thing we're waiting for.
     */
    public static void pollUiThread(final Callable<Boolean> criteria) throws Exception {
        assert ThreadUtils.runningOnUiThread();
        boolean isSatisfied = criteria.call();
        TimeoutTimer timer = new TimeoutTimer(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        while (!isSatisfied && !timer.isTimedOut()) {
            NestedSystemMessageHandler.runSingleNestedLooperTask(Looper.myQueue());
            isSatisfied = criteria.call();
        }
        Assert.assertTrue(isSatisfied);
    }
}