// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Process;
import android.os.SystemClock;
import android.webkit.WebView;

import org.chromium.base.Log;

import java.util.concurrent.LinkedBlockingQueue;

/**
 * This activity is designed for startup time testing of the WebView.
 */
public class StartupTimeActivity extends Activity {
    private static final String TAG = "WebViewShell";
    // Only records the tasks that affect the rendering performance of 30 FPS.
    private static final long MIN_TIME_TO_RECORD_MS = 33;
    private static final long TIME_TO_FINISH_APP_MS = 5000;

    private LinkedBlockingQueue<Long> mEventQueue = new LinkedBlockingQueue<>();

    private boolean mFinished;
    // Keep track of the time that the last task was run.
    private long mLastTaskTimeMs = -1;

    private Handler mHandler;

    private Runnable mUiBlockingTaskTracker = new Runnable() {
        @Override
        public void run() {
            if (mFinished) return;
            long now = System.currentTimeMillis();
            if (mLastTaskTimeMs != -1) {
                // The diff between current time and last task time is approximately
                // the time other UI tasks were run.
                long gap = now - mLastTaskTimeMs;
                if (gap > MIN_TIME_TO_RECORD_MS) {
                    try {
                        mEventQueue.put(gap);
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            }
            mLastTaskTimeMs = now;
            // Self-posting the current task to track future UI blocking tasks.
            mHandler.post(mUiBlockingTaskTracker);
        }
    };

    private Runnable mFinishTask = new Runnable() {
        @Override
        public void run() {
            mFinished = true;
            StringBuilder sb = new StringBuilder();
            while (true) {
                Long gap = mEventQueue.poll();
                if (gap == null) break;
                sb.append(gap);
                if (mEventQueue.peek() != null) sb.append(", ");
            }
            Log.i(TAG, "UI blocking times in startup (ms): " + sb.toString());
            finish();
            // Automatically clean up WebView to measure WebView startup cleanly
            // next time.
            Process.killProcess(Process.myPid());
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(
                getResources().getString(R.string.title_activity_startup_time));
        mHandler = new Handler();
        mUiBlockingTaskTracker.run();
        long t1 = SystemClock.elapsedRealtime();
        WebView webView = new WebView(this);
        setContentView(webView);
        long t2 = SystemClock.elapsedRealtime();
        mHandler.postDelayed(mFinishTask, TIME_TO_FINISH_APP_MS);
        Log.i(TAG, "WebViewStartupTimeMillis=" + (t2 - t1));
    }

}

