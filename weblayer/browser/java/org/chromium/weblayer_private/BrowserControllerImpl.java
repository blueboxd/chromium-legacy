// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IDownloadCallbackClient;
import org.chromium.weblayer_private.aidl.IFullscreenCallbackClient;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Implementation of IBrowserController.
 */
@JNINamespace("weblayer")
public final class BrowserControllerImpl extends IBrowserController.Stub {
    private static int sNextId = 1;
    private long mNativeBrowserController;

    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private BrowserCallbackProxy mBrowserCallbackProxy;
    private NavigationControllerImpl mNavigationController;
    private DownloadCallbackProxy mDownloadCallbackProxy;
    private FullscreenCallbackProxy mFullscreenCallbackProxy;
    private ViewAndroidDelegate mViewAndroidDelegate;
    // BrowserFragmentControllerImpl this BrowserControllerImpl is in. This is only null during
    // creation.
    private BrowserFragmentControllerImpl mFragment;
    private NewBrowserCallbackProxy mNewBrowserCallbackProxy;
    private IBrowserControllerClient mClient;
    private final int mId;

    private static class InternalAccessDelegateImpl
            implements ViewEventSink.InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {}
    }

    public BrowserControllerImpl(ProfileImpl profile, WindowAndroid windowAndroid) {
        mId = ++sNextId;
        init(profile, windowAndroid,
                BrowserControllerImplJni.get().createBrowserController(
                        profile.getNativeProfile(), this));
    }

    /**
     * This constructor is called when the native side triggers creation of a BrowserControllerImpl
     * (as happens with popups).
     */
    public BrowserControllerImpl(
            ProfileImpl profile, WindowAndroid windowAndroid, long nativeBrowserController) {
        mId = ++sNextId;
        BrowserControllerImplJni.get().setJavaImpl(
                nativeBrowserController, BrowserControllerImpl.this);
        init(profile, windowAndroid, nativeBrowserController);
    }

    private void init(
            ProfileImpl profile, WindowAndroid windowAndroid, long nativeBrowserController) {
        mProfile = profile;
        mNativeBrowserController = nativeBrowserController;
        mWebContents = BrowserControllerImplJni.get().getWebContents(
                mNativeBrowserController, BrowserControllerImpl.this);
        mViewAndroidDelegate = new ViewAndroidDelegate(null) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
                BrowserFragmentViewController viewController = getViewController();
                if (viewController != null) {
                    viewController.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
                }
            }
        };
        mWebContents.initialize("", mViewAndroidDelegate, new InternalAccessDelegateImpl(),
                windowAndroid, WebContents.createDefaultInternalsHolder());
    }

    public ProfileImpl getProfile() {
        return mProfile;
    }

    public IBrowserControllerClient getClient() {
        return mClient;
    }

    /**
     * Sets the BrowserFragmentControllerImpl this BrowserControllerImpl is contained in.
     */
    public void attachToFragment(BrowserFragmentControllerImpl fragment) {
        mFragment = fragment;
        mWebContents.setTopLevelNativeWindow(fragment.getWindowAndroid());
        mViewAndroidDelegate.setContainerView(fragment.getViewAndroidDelegateContainerView());
        SelectionPopupController.fromWebContents(mWebContents)
                .setActionModeCallback(new ActionModeCallback(mWebContents));
    }

    public BrowserFragmentControllerImpl getFragment() {
        return mFragment;
    }

    @Override
    public void setNewBrowsersEnabled(boolean enable) {
        if (enable && mNewBrowserCallbackProxy == null) {
            mNewBrowserCallbackProxy = new NewBrowserCallbackProxy(this);
        } else if (!enable && mNewBrowserCallbackProxy != null) {
            mNewBrowserCallbackProxy.destroy();
            mNewBrowserCallbackProxy = null;
        }
    }

    @Override
    public int getId() {
        return mId;
    }

    /**
     * Called when this BrowserControllerImpl becomes the active BrowserControllerImpl.
     */
    public void onDidGainActive(long topControlsContainerViewHandle) {
        // attachToFragment() must be called before activate().
        assert mFragment != null;
        BrowserControllerImplJni.get().setTopControlsContainerView(mNativeBrowserController,
                BrowserControllerImpl.this, topControlsContainerViewHandle);
        mWebContents.onShow();
    }
    /**
     * Called when this BrowserControllerImpl is no longer the active BrowserControllerImpl.
     */
    public void onDidLoseActive() {
        mWebContents.onHide();
        BrowserControllerImplJni.get().setTopControlsContainerView(
                mNativeBrowserController, BrowserControllerImpl.this, 0);
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    long getNativeBrowserController() {
        return mNativeBrowserController;
    }

    @Override
    public NavigationControllerImpl createNavigationController(INavigationControllerClient client) {
        // This should only be called once.
        assert mNavigationController == null;
        mNavigationController = new NavigationControllerImpl(this, client);
        return mNavigationController;
    }

    @Override
    public void setClient(IBrowserControllerClient client) {
        mClient = client;
        mBrowserCallbackProxy = new BrowserCallbackProxy(mNativeBrowserController, client);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        if (client != null) {
            if (mDownloadCallbackProxy == null) {
                mDownloadCallbackProxy =
                        new DownloadCallbackProxy(mNativeBrowserController, client);
            } else {
                mDownloadCallbackProxy.setClient(client);
            }
        } else if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
    }

    @Override
    public void setFullscreenCallbackClient(IFullscreenCallbackClient client) {
        if (client != null) {
            if (mFullscreenCallbackProxy == null) {
                mFullscreenCallbackProxy =
                        new FullscreenCallbackProxy(mNativeBrowserController, client);
            } else {
                mFullscreenCallbackProxy.setClient(client);
            }
        } else if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IObjectWrapper callback) {
        Callback<String> nativeCallback = new Callback<String>() {
            @Override
            public void onResult(String result) {
                ValueCallback<String> unwrappedCallback =
                        (ValueCallback<String>) ObjectWrapper.unwrap(callback, ValueCallback.class);
                if (unwrappedCallback != null) {
                    unwrappedCallback.onReceiveValue(result);
                }
            }
        };
        BrowserControllerImplJni.get().executeScript(
                mNativeBrowserController, script, useSeparateIsolate, nativeCallback);
    }

    public void destroy() {
        if (mBrowserCallbackProxy != null) {
            mBrowserCallbackProxy.destroy();
            mBrowserCallbackProxy = null;
        }
        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
        if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
        if (mNewBrowserCallbackProxy != null) {
            mNewBrowserCallbackProxy.destroy();
            mNewBrowserCallbackProxy = null;
        }
        mNavigationController = null;
        BrowserControllerImplJni.get().deleteBrowserController(mNativeBrowserController);
        mNativeBrowserController = 0;
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        BrowserFragmentViewController viewController = getViewController();
        return viewController != null && viewController.doBrowserControlsShrinkRendererSize();
    }

    /**
     * Returns the BrowserFragmentViewController for this BrowserControllerImpl, but only if this
     * is the active BrowserControllerImpl.
     */
    private BrowserFragmentViewController getViewController() {
        return (mFragment.getActiveBrowserController() == this) ? mFragment.getViewController()
                                                                : null;
    }

    @NativeMethods
    interface Natives {
        long createBrowserController(long profile, BrowserControllerImpl caller);
        void setJavaImpl(long nativeBrowserControllerImpl, BrowserControllerImpl impl);
        void setTopControlsContainerView(long nativeBrowserControllerImpl,
                BrowserControllerImpl caller, long nativeTopControlsContainerView);
        void deleteBrowserController(long browserController);
        WebContents getWebContents(long nativeBrowserControllerImpl, BrowserControllerImpl caller);
        void executeScript(long nativeBrowserControllerImpl, String script,
                boolean useSeparateIsolate, Callback<String> callback);
    }
}
