// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.external_intents.RedirectHandler;

/**
 * This class glues RedirectHandler instances to Tabs.
 */
public class RedirectHandlerTabHelper extends EmptyTabObserver implements UserData {
    private static final Class<RedirectHandlerTabHelper> USER_DATA_KEY =
            RedirectHandlerTabHelper.class;

    private RedirectHandler mRedirectHandler;

    /**
     * Returns {@link RedirectHandler} that hangs on to a given {@link Tab}.
     * If not present, creates a new instance and associate it with the {@link UserDataHost}
     * that the {@link Tab} manages.
     * @param tab Tab instance that the RedirectHandler hangs on to.
     * @return RedirectHandler for a given Tab.
     */
    public static RedirectHandler getOrCreateHandlerFor(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        RedirectHandlerTabHelper helper = host.getUserData(USER_DATA_KEY);
        if (helper == null) {
            helper = new RedirectHandlerTabHelper();
            host.setUserData(USER_DATA_KEY, helper);
            tab.addObserver(helper);
        }
        return helper.mRedirectHandler;
    }

    /**
     * @return {@link RedirectHandler} hanging to the given {@link Tab},
     *     or {@code null} if there is no instance available.
     */
    @Nullable
    public static RedirectHandler getHandlerFor(Tab tab) {
        RedirectHandlerTabHelper helper = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (helper == null) return null;
        return helper.mRedirectHandler;
    }

    /**
     * Replace {@link RedirectHandler} instance for the Tab with the new one.
     * @return Old {@link RedirectHandler} associated with the Tab. Could be {@code null}.
     */
    public static RedirectHandler swapHandlerFor(Tab tab, @Nullable RedirectHandler newHandler) {
        UserDataHost host = tab.getUserDataHost();
        RedirectHandlerTabHelper oldHelper = host.getUserData(USER_DATA_KEY);
        if (newHandler != null) {
            RedirectHandlerTabHelper newHelper = new RedirectHandlerTabHelper(newHandler);
            host.setUserData(USER_DATA_KEY, newHelper);
        } else {
            host.removeUserData(USER_DATA_KEY);
        }

        if (oldHelper == null) return null;
        return oldHelper.mRedirectHandler;
    }

    private RedirectHandlerTabHelper() {
        mRedirectHandler = RedirectHandler.create();
    }

    private RedirectHandlerTabHelper(RedirectHandler handler) {
        mRedirectHandler = handler;
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        mRedirectHandler.clear();
    }

    /**
     * Wrapper around RedirectHandler#updateIntent() that supplies //chrome-level params.
     */
    public static void updateIntentInTab(Tab tab, Intent intent) {
        RedirectHandlerTabHelper.getOrCreateHandlerFor(tab).updateIntent(intent,
                LaunchIntentDispatcher.isCustomTabIntent(intent),
                IntentUtils.safeGetBooleanExtra(intent,
                        CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, false),
                ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_EXTERNAL_LINK_HANDLING));
    }
}
