// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.content_public.browser.WebContents;

/**
 * Communicates between CookieControlsController (C++ backend) and PageInfoView (Java UI).
 */
public class CookieControlsBridge {
    private long mNativeCookieControlsBridge;
    private CookieControlsObserver mObserver;

    /**
     * Initializes a CookieControlsBridge instance.
     * @param observer An observer to call with updates from the cookie controller.
     * @param webContents The WebContents instance to observe.
     */
    public CookieControlsBridge(CookieControlsObserver observer, WebContents webContents) {
        mObserver = observer;
        mNativeCookieControlsBridge =
                CookieControlsBridgeJni.get().init(CookieControlsBridge.this, webContents);
    }

    public void setThirdPartyCookieBlockingEnabledForSite(boolean blockCookies) {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().setThirdPartyCookieBlockingEnabledForSite(
                    mNativeCookieControlsBridge, blockCookies);
        }
    }

    public void onUiClosing() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().onUiClosing(mNativeCookieControlsBridge);
        }
    }

    /**
     * Destroys the native counterpart of this class.
     */
    public void destroy() {
        if (mNativeCookieControlsBridge != 0) {
            CookieControlsBridgeJni.get().destroy(
                    mNativeCookieControlsBridge, CookieControlsBridge.this);
            mNativeCookieControlsBridge = 0;
        }
    }

    public static boolean isCookieControlsEnabled(Profile profile) {
        return CookieControlsBridgeJni.get().isCookieControlsEnabled(profile);
    }

    @CalledByNative
    private void onCookieBlockingStatusChanged(
            @CookieControlsStatus int status, @CookieControlsEnforcement int enforcement) {
        mObserver.onCookieBlockingStatusChanged(status, enforcement);
    }

    @CalledByNative
    private void onBlockedCookiesCountChanged(int blockedCookies) {
        mObserver.onBlockedCookiesCountChanged(blockedCookies);
    }

    @NativeMethods
    interface Natives {
        long init(CookieControlsBridge caller, WebContents webContents);
        void destroy(long nativeCookieControlsBridge, CookieControlsBridge caller);
        void setThirdPartyCookieBlockingEnabledForSite(
                long nativeCookieControlsBridge, boolean blockCookies);
        void onUiClosing(long nativeCookieControlsBridge);
        boolean isCookieControlsEnabled(Profile profile);
    }
}
