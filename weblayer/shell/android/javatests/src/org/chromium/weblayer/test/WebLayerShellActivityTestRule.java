// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.json.JSONObject;
import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationObserver;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.lang.reflect.Field;
import java.util.concurrent.TimeoutException;

/**
 * ActivityTestRule for WebLayerShellActivity.
 *
 * Test can use this ActivityTestRule to launch or get WebLayerShellActivity.
 */
public class WebLayerShellActivityTestRule extends ActivityTestRule<WebLayerShellActivity> {
    private static final class NavigationWaiter {
        private String mUrl;
        private BrowserController mController;
        private boolean mNavigationComplete;
        private boolean mDoneLoading;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        private NavigationObserver mNavigationObserver = new NavigationObserver() {
            @Override
            public void navigationCompleted(Navigation navigation) {
                if (navigation.getUri().toString().equals(mUrl)) {
                    mNavigationComplete = true;
                    checkComplete();
                }
            }
        };

        private BrowserObserver mBrowserObserver = new BrowserObserver() {
            @Override
            public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {
                mDoneLoading = !isLoading;
                checkComplete();
            }
        };

        public NavigationWaiter(String url, BrowserController controller) {
            mUrl = url;
            mController = controller;
        }

        public void navigateAndWait() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mController.addObserver(mBrowserObserver);
                mController.getNavigationController().addObserver(mNavigationObserver);
                mController.getNavigationController().navigate(Uri.parse(mUrl));
            });
            try {
                mCallbackHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mController.removeObserver(mBrowserObserver);
                mController.getNavigationController().removeObserver(mNavigationObserver);
            });
        }

        private void checkComplete() {
            if (mNavigationComplete && mDoneLoading) {
                mCallbackHelper.notifyCalled();
            }
        }
    }

    private static final class JSONCallbackHelper extends CallbackHelper {
        private JSONObject mResult;

        public JSONObject getResult() {
            return mResult;
        }

        public void notifyCalled(JSONObject result) {
            mResult = result;
            notifyCalled();
        }
    }

    public WebLayerShellActivityTestRule() {
        super(WebLayerShellActivity.class, false, false);
    }

    /**
     * Starts the WebLayer activity and loads the given URL.
     */
    public WebLayerShellActivity launchShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Prevent URL from being loaded on start.
        intent.putExtra(WebLayerShellActivity.EXTRA_NO_LOAD, true);
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        WebLayerShellActivity.class));
        WebLayerShellActivity activity = launchActivity(intent);
        Assert.assertNotNull(activity);
        if (url != null) navigateAndWait(url);
        return activity;
    }

    /**
     * Loads the given URL in the shell.
     */
    public void navigateAndWait(String url) {
        NavigationWaiter waiter = new NavigationWaiter(url, getActivity().getBrowserController());
        waiter.navigateAndWait();
    }

    /**
     * Rotates the Activity, blocking until the Activity is re-created.
     * After calling this, getActivity() returns the new Activity.
     */
    public void rotateActivity() {
        Activity activity = getActivity();

        ActivityMonitor monitor = new ActivityMonitor(WebLayerShellActivity.class.getName(),
                null, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(monitor);

        int current = activity.getResources().getConfiguration().orientation;
        int requested = current == Configuration.ORIENTATION_LANDSCAPE ?
                ActivityInfo.SCREEN_ORIENTATION_PORTRAIT :
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        activity.setRequestedOrientation(requested);
        CriteriaHelper.pollUiThread(() ->
            monitor.getLastActivity() != null && monitor.getLastActivity() != activity
        );
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);

        // There is no way to rotate the activity using ActivityTestRule or even notify it.
        // So we have to hack...
        try {
            Field field = ActivityTestRule.class.getDeclaredField("mActivity");
            field.setAccessible(true);
            field.set(this, monitor.getLastActivity());
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Executes the script passed in and waits for the result.
     */
    public JSONObject executeScriptSync(String script) {
        JSONCallbackHelper callbackHelper = new JSONCallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().getBrowserController().executeScript(
                    script, (JSONObject result) -> { callbackHelper.notifyCalled(result); });
        });
        try {
            callbackHelper.waitForCallback(count);
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
        return callbackHelper.getResult();
    }
}
