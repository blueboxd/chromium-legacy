// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.util.Scheduler;

import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.CancellationException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link AsyncTask}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncTaskThreadTest {
    private static final String TAG = "AsyncTaskThreadTest";
    private static final boolean DEBUG = false;

    private static class BlockAndGetFeedDataTask extends AsyncTask<Boolean> {
        private LinkedBlockingQueue<Boolean> mIncomingQueue = new LinkedBlockingQueue<Boolean>();
        private LinkedBlockingQueue<Boolean> mOutgoingQueue = new LinkedBlockingQueue<Boolean>();

        @Override
        protected Boolean doInBackground() {
            if (DEBUG) Log.i(TAG, "doInBackground");
            mOutgoingQueue.add(true);
            return blockAndGetFeedData();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (DEBUG) Log.i(TAG, "onPostExecute: " + result);
        }

        public void feedData(Boolean data) {
            mIncomingQueue.add(data);
        }

        private Boolean blockAndGetFeedData() {
            try {
                return mIncomingQueue.poll(3, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                return false;
            }
        }

        public void blockUntilDoInBackgroundStarts() throws Exception {
            mOutgoingQueue.poll(3, TimeUnit.SECONDS);
        }
    }

    private final RoboExecutorService mRoboExecutorService = new RoboExecutorService();
    private final Scheduler mBackgroundScheduler = Robolectric.getBackgroundThreadScheduler();

    @Rule
    public ExpectedException thrown = ExpectedException.none();

    public AsyncTaskThreadTest() {
        if (DEBUG) ShadowLog.stream = System.out;
    }

    @Before
    public void setUp() {
        mBackgroundScheduler.pause();
    }

    @Test
    @SmallTest
    public void testCancel_ReturnsFalseOnceTaskFinishes() throws Exception {
        BlockAndGetFeedDataTask task = new BlockAndGetFeedDataTask();
        // This test requires robo executor service such that we can run
        // one background task.
        task.executeOnExecutor(mRoboExecutorService);

        // We feed the background thread, then cancel.
        task.feedData(true);
        mBackgroundScheduler.runOneTask();

        // Cannot cancel. The task is already run.
        assertFalse(task.cancel(false));
        assertTrue(task.get());
    }

    @Test
    @SmallTest
    public void testCancel_CanReturnTrueEvenAfterTaskStarts() throws Exception {
        BlockAndGetFeedDataTask task = new BlockAndGetFeedDataTask();
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // The task has started.
        task.blockUntilDoInBackgroundStarts();
        // This reflects FutureTask#cancel() behavior. Note that the task is
        // started but cancel can still return true.
        assertTrue(task.cancel(false));
        task.feedData(true);

        // get() will raise an exception although the task is run.
        try {
            task.get();
            Assert.fail();
        } catch (CancellationException e) {
            // expected
        }
    }
}
