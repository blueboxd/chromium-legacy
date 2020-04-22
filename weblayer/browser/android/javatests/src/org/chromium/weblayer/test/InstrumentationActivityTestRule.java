// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.text.TextUtils;

import androidx.fragment.app.Fragment;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.lang.reflect.Field;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * ActivityTestRule for InstrumentationActivity.
 *
 * Test can use this ActivityTestRule to launch or get InstrumentationActivity.
 */
public class InstrumentationActivityTestRule extends ActivityTestRule<InstrumentationActivity> {
    private static final String COMMAND_LINE_FILE = "weblayer-command-line";

    @Rule
    private EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

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

    public InstrumentationActivityTestRule() {
        super(InstrumentationActivity.class, false, false);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        Statement testServer = super.apply(mTestServerRule.apply(base, description), description);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    // The CommandLine instance we have here will not be picked up in the
                    // implementation since they use different class loaders, so we need to write
                    // all the switches to the WebLayer command line file.
                    try (Writer writer = new OutputStreamWriter(
                                 InstrumentationRegistry.getInstrumentation()
                                         .getTargetContext()
                                         .openFileOutput(COMMAND_LINE_FILE, Context.MODE_PRIVATE),
                                 "UTF-8")) {
                        writer.write(TextUtils.join(" ", CommandLine.getJavaSwitchesOrNull()));
                    }

                    testServer.evaluate();
                } finally {
                    new File(InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getFilesDir(),
                            COMMAND_LINE_FILE)
                            .delete();
                }
            }
        };
    }

    public WebLayer getWebLayer() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return WebLayer.loadSync(
                    InstrumentationRegistry.getTargetContext().getApplicationContext());
        });
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle. This does not create and load
     * WebLayer.
     */
    public InstrumentationActivity launchShell(Bundle extras) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtras(extras);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        InstrumentationActivity.class));
        return launchActivity(intent);
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle and completely loads the given URL
     * (this calls navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url, Bundle extras) {
        InstrumentationActivity activity = launchShell(extras);
        Assert.assertNotNull(activity);
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> activity.loadWebLayerSync(activity.getApplicationContext()));
        } catch (ExecutionException e) {
            throw new RuntimeException(e);
        }
        if (url != null) navigateAndWait(url);
        return activity;
    }

    /**
     * Starts the WebLayer activity and completely loads the given URL (this calls
     * navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url) {
        return launchShellWithUrl(url, new Bundle());
    }

    /**
     * Loads the given URL in the shell.
     */
    public void navigateAndWait(String url) {
        navigateAndWait(getActivity().getTab(), url, true /* waitForPaint */);
    }

    public void navigateAndWait(Tab tab, String url, boolean waitForPaint) {
        (new NavigationWaiter(url, tab, false /* expectFailure */, waitForPaint)).navigateAndWait();
    }

    /**
     * Loads the given URL in the shell, expecting failure.
     */
    public void navigateAndWaitForFailure(String url) {
        navigateAndWaitForFailure(getActivity().getTab(), url, true /* waitForPaint */);
    }

    public void navigateAndWaitForFailure(Tab tab, String url, boolean waitForPaint) {
        (new NavigationWaiter(url, tab, true /* expectFailure */, waitForPaint)).navigateAndWait();
    }

    /**
     * Recreates the Activity, blocking until finished.
     * After calling this, getActivity() returns the new Activity.
     */
    public void recreateActivity() {
        Activity activity = getActivity();

        ActivityMonitor monitor =
                new ActivityMonitor(InstrumentationActivity.class.getName(), null, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(monitor);

        TestThreadUtils.runOnUiThreadBlocking(activity::recreate);

        CriteriaHelper.pollUiThread(
                () -> monitor.getLastActivity() != null && monitor.getLastActivity() != activity);
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
    public JSONObject executeScriptSync(String script, boolean useSeparateIsolate, Tab tab) {
        JSONCallbackHelper callbackHelper = new JSONCallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab scriptTab = tab == null ? getActivity().getBrowser().getActiveTab() : tab;
            scriptTab.executeScript(script, useSeparateIsolate,
                    (JSONObject result) -> { callbackHelper.notifyCalled(result); });
        });
        try {
            callbackHelper.waitForCallback(count);
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
        return callbackHelper.getResult();
    }

    public JSONObject executeScriptSync(String script, boolean useSeparateIsolate) {
        return executeScriptSync(script, useSeparateIsolate, null);
    }

    public int executeScriptAndExtractInt(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getInt(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public String executeScriptAndExtractString(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getString(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public boolean executeScriptAndExtractBoolean(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getBoolean(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public InstrumentationActivity launchWithProfile(String profileName) {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, profileName);
        String url = "data:text,foo";
        return launchShellWithUrl(url, extras);
    }

    public EmbeddedTestServer getTestServer() {
        return mTestServerRule.getServer();
    }

    public EmbeddedTestServerRule getTestServerRule() {
        return mTestServerRule;
    }

    public String getTestDataURL(String path) {
        return getTestServer().getURL("/weblayer/test/data/" + path);
    }

    // Returns the URL that is currently being displayed to the user.
    public String getCurrentDisplayUrl() {
        return getActivity().getCurrentDisplayUrl();
    }

    public void setRetainInstance(boolean retain) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getActivity().setRetainInstance(retain));
    }

    public Fragment getFragment() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> getActivity().getFragment());
    }

    public boolean setCookie(CookieManager cookieManager, Uri uri, String value) throws Exception {
        Boolean[] resultHolder = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cookieManager.setCookie(uri, value, (Boolean result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }

    public String getCookie(CookieManager cookieManager, Uri uri) throws Exception {
        String[] resultHolder = new String[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cookieManager.getCookie(uri, (String result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }
}
