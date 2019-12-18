// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests that when DeferredStartupHandler#queueDeferredTasksOnIdleHandler() is run that the
 * activity's tab has finished loading.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabDeferredStartupTest {
    static class PageLoadFinishedTabObserver extends EmptyTabObserver {
        private boolean mIsPageLoadFinished;

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            mIsPageLoadFinished = true;
        }

        public boolean isPageLoadFinished() {
            return mIsPageLoadFinished;
        }
    }

    static class InitialTabCreationObserver extends CustomTabActivityTabProvider.Observer {
        private TabObserver mObserver;

        public InitialTabCreationObserver(TabObserver observer) {
            mObserver = observer;
        }

        @Override
        public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
            tab.addObserver(mObserver);
        }
    }

    static class NewTabObserver extends EmptyTabModelSelectorObserver
            implements ApplicationStatus.ActivityStateListener, InflationObserver {
        private BaseCustomTabActivity mActivity;
        private TabObserver mObserver;

        public NewTabObserver(TabObserver observer) {
            mObserver = observer;
        }

        @Override
        public void onNewTabCreated(Tab tab) {
            tab.addObserver(mObserver);
        }

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.CREATED && activity instanceof BaseCustomTabActivity
                    && mActivity == null) {
                mActivity = (BaseCustomTabActivity) activity;
                mActivity.getLifecycleDispatcher().register(this);
            }
        }

        @Override
        public void onPreInflationStartup() {
            BaseCustomTabActivityComponent baseCustomTabActivityComponent =
                    (BaseCustomTabActivityComponent) mActivity.getComponent();
            baseCustomTabActivityComponent.resolveTabProvider().addObserver(
                    new InitialTabCreationObserver(mObserver));
        }

        @Override
        public void onPostInflationStartup() {}
    }

    static class PageIsLoadedDeferredStartupHandler extends DeferredStartupHandler {
        public PageIsLoadedDeferredStartupHandler(
                PageLoadFinishedTabObserver observer, CallbackHelper helper) {
            mObserver = observer;
            mHelper = helper;
        }

        @Override
        public void queueDeferredTasksOnIdleHandler() {
            Assert.assertTrue("Page is yet to finish loading.", mObserver.isPageLoadFinished());

            mHelper.notifyCalled();

            super.queueDeferredTasksOnIdleHandler();
        }

        private CallbackHelper mHelper;
        private PageLoadFinishedTabObserver mObserver;
    }

    @ClassParameter
    public static List<ParameterSet> sClassParams = Arrays.asList(
            new ParameterSet().value(ActivityType.WEBAPP).name("Webapp"),
            new ParameterSet().value(ActivityType.CUSTOM_TAB).name("CustomTab"),
            new ParameterSet().value(ActivityType.TRUSTED_WEB_ACTIVITY).name("TrustedWebActivity"));

    private @ActivityType int mActivityType;

    @Rule
    public final ChromeActivityTestRule mActivityTestRule;

    public CustomTabDeferredStartupTest(@ActivityType int activityType) {
        mActivityType = activityType;
        mActivityTestRule = (activityType == ActivityType.WEBAPP) ? new WebappActivityTestRule()
                                                                  : new CustomTabActivityTestRule();
    }

    private void launchActivity() throws TimeoutException {
        if (mActivityType == ActivityType.WEBAPP) {
            launchWebapp((WebappActivityTestRule) mActivityTestRule);
            return;
        }

        CustomTabActivityTestRule customTabActivityTestRule =
                (CustomTabActivityTestRule) mActivityTestRule;
        if (mActivityType == ActivityType.CUSTOM_TAB) {
            launchCct(customTabActivityTestRule);
            return;
        }
        launchTwa(customTabActivityTestRule);
    }

    private void launchWebapp(WebappActivityTestRule activityTestRule) {
        activityTestRule.startWebappActivity();
    }

    private void launchCct(CustomTabActivityTestRule activityTestRule) {
        activityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
    }

    private void launchTwa(CustomTabActivityTestRule activityTestRule) throws TimeoutException {
        String packageName = InstrumentationRegistry.getTargetContext().getPackageName();
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent("about:blank");
        TrustedWebActivityTestUtil.spoofVerification(packageName, "about:blank");
        TrustedWebActivityTestUtil.createSession(intent, packageName);
        activityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @LargeTest
    public void testPageIsLoadedOnDeferredStartup() throws Exception {
        PageLoadFinishedTabObserver tabObserver = new PageLoadFinishedTabObserver();
        NewTabObserver newTabObserver = new NewTabObserver(tabObserver);
        TabModelSelectorBase.setObserverForTests(newTabObserver);
        ApplicationStatus.registerStateListenerForAllActivities(newTabObserver);
        CallbackHelper helper = new CallbackHelper();
        PageIsLoadedDeferredStartupHandler handler =
                new PageIsLoadedDeferredStartupHandler(tabObserver, helper);
        DeferredStartupHandler.setInstanceForTests(handler);
        launchActivity();
        helper.waitForCallback(0);
    }
}
