// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.RectF;
import android.os.Build;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderImpl;
import org.chromium.components.find_in_page.FindInPageBridge;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindResultBar;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of ITab.
 */
@JNINamespace("weblayer")
public final class TabImpl extends ITab.Stub {
    private static int sNextId = 1;
    private long mNativeTab;

    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private TabCallbackProxy mTabCallbackProxy;
    private NavigationControllerImpl mNavigationController;
    private DownloadCallbackProxy mDownloadCallbackProxy;
    private ErrorPageCallbackProxy mErrorPageCallbackProxy;
    private FullscreenCallbackProxy mFullscreenCallbackProxy;
    private ViewAndroidDelegate mViewAndroidDelegate;
    // BrowserImpl this TabImpl is in. This is only null during creation.
    private BrowserImpl mBrowser;
    /**
     * The AutofillProvider that integrates with system-level autofill. This is null until
     * updateFromBrowser() is invoked.
     */
    private AutofillProvider mAutofillProvider;
    private NewTabCallbackProxy mNewTabCallbackProxy;
    private ITabClient mClient;
    private final int mId;

    private IFindInPageCallbackClient mFindInPageCallbackClient;
    private FindInPageBridge mFindInPageBridge;
    private FindResultBar mFindResultBar;
    // See usage note in {@link #onFindResultAvailable}.
    boolean mWaitingForMatchRects;

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

    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid) {
        mId = ++sNextId;
        init(profile, windowAndroid, TabImplJni.get().createTab(profile.getNativeProfile(), this));
    }

    /**
     * This constructor is called when the native side triggers creation of a TabImpl
     * (as happens with popups and other scenarios).
     */
    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mId = ++sNextId;
        TabImplJni.get().setJavaImpl(nativeTab, TabImpl.this);
        init(profile, windowAndroid, nativeTab);
    }

    private void init(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mProfile = profile;
        mNativeTab = nativeTab;
        mWebContents = TabImplJni.get().getWebContents(mNativeTab, TabImpl.this);
        mViewAndroidDelegate = new ViewAndroidDelegate(null) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
                BrowserViewController viewController = getViewController();
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

    public ITabClient getClient() {
        return mClient;
    }

    /**
     * Sets the BrowserImpl this TabImpl is contained in.
     */
    public void attachToBrowser(BrowserImpl browser) {
        mBrowser = browser;
        updateFromBrowser();
        SelectionPopupController.fromWebContents(mWebContents)
                .setActionModeCallback(new ActionModeCallback(mWebContents));
    }

    public void updateFromBrowser() {
        mWebContents.setTopLevelNativeWindow(mBrowser.getWindowAndroid());
        mViewAndroidDelegate.setContainerView(mBrowser.getViewAndroidDelegateContainerView());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (mBrowser.getContext() == null) {
                // The Context and ViewContainer in which Autofill was previously operating have
                // gone away, so tear down |mAutofillProvider|.
                mAutofillProvider = null;
            } else {
                // Set up |mAutofillProvider| to operate in the new Context.
                mAutofillProvider = new AutofillProviderImpl(
                        mBrowser.getContext(), mBrowser.getViewAndroidDelegateContainerView());
                mAutofillProvider.setWebContents(mWebContents);
            }

            TabImplJni.get().onAutofillProviderChanged(mNativeTab, mAutofillProvider);
        }
    }

    public BrowserImpl getBrowser() {
        return mBrowser;
    }

    @Override
    public void setNewTabsEnabled(boolean enable) {
        StrictModeWorkaround.apply();
        if (enable && mNewTabCallbackProxy == null) {
            mNewTabCallbackProxy = new NewTabCallbackProxy(this);
        } else if (!enable && mNewTabCallbackProxy != null) {
            mNewTabCallbackProxy.destroy();
            mNewTabCallbackProxy = null;
        }
    }

    @Override
    public int getId() {
        StrictModeWorkaround.apply();
        return mId;
    }

    /**
     * Called when this TabImpl becomes the active TabImpl.
     */
    public void onDidGainActive(long topControlsContainerViewHandle) {
        // attachToFragment() must be called before activate().
        assert mBrowser != null;
        TabImplJni.get().setTopControlsContainerView(
                mNativeTab, TabImpl.this, topControlsContainerViewHandle);
        mWebContents.onShow();
    }
    /**
     * Called when this TabImpl is no longer the active TabImpl.
     */
    public void onDidLoseActive() {
        hideFindInPageUiAndNotifyClient();
        mWebContents.onHide();
        TabImplJni.get().setTopControlsContainerView(mNativeTab, TabImpl.this, 0);
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    public AutofillProvider getAutofillProvider() {
        return mAutofillProvider;
    }

    long getNativeTab() {
        return mNativeTab;
    }

    @Override
    public NavigationControllerImpl createNavigationController(INavigationControllerClient client) {
        StrictModeWorkaround.apply();
        // This should only be called once.
        assert mNavigationController == null;
        mNavigationController = new NavigationControllerImpl(this, client);
        return mNavigationController;
    }

    @Override
    public void setClient(ITabClient client) {
        StrictModeWorkaround.apply();
        mClient = client;
        mTabCallbackProxy = new TabCallbackProxy(mNativeTab, client);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mDownloadCallbackProxy == null) {
                mDownloadCallbackProxy = new DownloadCallbackProxy(mBrowser, mNativeTab, client);
            } else {
                mDownloadCallbackProxy.setClient(client);
            }
        } else if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
    }

    @Override
    public void setErrorPageCallbackClient(IErrorPageCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mErrorPageCallbackProxy == null) {
                mErrorPageCallbackProxy = new ErrorPageCallbackProxy(mNativeTab, client);
            } else {
                mErrorPageCallbackProxy.setClient(client);
            }
        } else if (mErrorPageCallbackProxy != null) {
            mErrorPageCallbackProxy.destroy();
            mErrorPageCallbackProxy = null;
        }
    }

    @Override
    public void setFullscreenCallbackClient(IFullscreenCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mFullscreenCallbackProxy == null) {
                mFullscreenCallbackProxy = new FullscreenCallbackProxy(mNativeTab, client);
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
        StrictModeWorkaround.apply();
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
        TabImplJni.get().executeScript(mNativeTab, script, useSeparateIsolate, nativeCallback);
    }

    @Override
    public boolean setFindInPageCallbackClient(IFindInPageCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client == null) {
            // Null now to avoid calling onFindEnded.
            mFindInPageCallbackClient = null;
            hideFindInPageUiAndNotifyClient();
            return true;
        }

        if (mFindInPageCallbackClient != null) return false;

        BrowserViewController controller = getViewController();
        if (controller == null) return false;

        mFindInPageCallbackClient = client;
        assert mFindInPageBridge == null;
        mFindInPageBridge = new FindInPageBridge(mWebContents);
        assert mFindResultBar == null;
        mFindResultBar =
                new FindResultBar(mBrowser.getContext(), controller.getWebContentsOverlayView(),
                        mBrowser.getWindowAndroid(), mFindInPageBridge);
        return true;
    }

    @Override
    public void findInPage(String searchText, boolean forward) {
        StrictModeWorkaround.apply();
        if (mFindInPageBridge == null) return;
        if (searchText.length() > 0) {
            mFindInPageBridge.startFinding(searchText, forward, false);
        } else {
            mFindInPageBridge.stopFinding(true);
        }
    }

    private void hideFindInPageUiAndNotifyClient() {
        if (mFindInPageBridge == null) return;

        mFindInPageBridge.stopFinding(true);

        mFindResultBar.dismiss();
        mFindResultBar = null;
        mFindInPageBridge.destroy();
        mFindInPageBridge = null;

        try {
            if (mFindInPageCallbackClient != null) mFindInPageCallbackClient.onFindEnded();
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    @CalledByNative
    private static RectF createRectF(float x, float y, float right, float bottom) {
        return new RectF(x, y, right, bottom);
    }

    @CalledByNative
    private static FindMatchRectsDetails createFindMatchRectsDetails(
            int version, int numRects, RectF activeRect) {
        return new FindMatchRectsDetails(version, numRects, activeRect);
    }

    @CalledByNative
    private static void setMatchRectByIndex(
            FindMatchRectsDetails findMatchRectsDetails, int index, RectF rect) {
        findMatchRectsDetails.rects[index] = rect;
    }

    @CalledByNative
    private void onFindResultAvailable(
            int numberOfMatches, int activeMatchOrdinal, boolean finalUpdate) {
        try {
            if (mFindInPageCallbackClient != null) {
                // The WebLayer API deals in indices instead of ordinals.
                mFindInPageCallbackClient.onFindResult(
                        numberOfMatches, activeMatchOrdinal - 1, finalUpdate);
            }
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }

        if (mFindResultBar != null) {
            mFindResultBar.onFindResult();
            if (finalUpdate) {
                if (numberOfMatches > 0) {
                    mWaitingForMatchRects = true;
                    mFindInPageBridge.requestFindMatchRects(mFindResultBar.getRectsVersion());
                } else {
                    // Match rects results that correlate to an earlier call to
                    // requestFindMatchRects might still come in, so set this sentinel to false to
                    // make sure we ignore them instead of showing stale results.
                    mWaitingForMatchRects = false;
                    mFindResultBar.clearMatchRects();
                }
            }
        }
    }

    @CalledByNative
    private void onFindMatchRectsAvailable(FindMatchRectsDetails matchRects) {
        if (mFindResultBar != null && mWaitingForMatchRects) {
            mFindResultBar.setMatchRects(
                    matchRects.version, matchRects.rects, matchRects.activeRect);
        }
    }

    public void destroy() {
        if (mTabCallbackProxy != null) {
            mTabCallbackProxy.destroy();
            mTabCallbackProxy = null;
        }
        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
        if (mErrorPageCallbackProxy != null) {
            mErrorPageCallbackProxy.destroy();
            mErrorPageCallbackProxy = null;
        }
        if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
        if (mNewTabCallbackProxy != null) {
            mNewTabCallbackProxy.destroy();
            mNewTabCallbackProxy = null;
        }
        hideFindInPageUiAndNotifyClient();
        mFindInPageCallbackClient = null;
        mNavigationController = null;
        TabImplJni.get().deleteTab(mNativeTab);
        mNativeTab = 0;
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        BrowserViewController viewController = getViewController();
        return viewController != null && viewController.doBrowserControlsShrinkRendererSize();
    }

    /**
     * Returns the BrowserViewController for this TabImpl, but only if this
     * is the active TabImpl.
     */
    private BrowserViewController getViewController() {
        return (mBrowser.getActiveTab() == this) ? mBrowser.getViewController() : null;
    }

    @NativeMethods
    interface Natives {
        long createTab(long profile, TabImpl caller);
        void setJavaImpl(long nativeTabImpl, TabImpl impl);
        void onAutofillProviderChanged(long nativeTabImpl, AutofillProvider autofillProvider);
        void setTopControlsContainerView(
                long nativeTabImpl, TabImpl caller, long nativeTopControlsContainerView);
        void deleteTab(long tab);
        WebContents getWebContents(long nativeTabImpl, TabImpl caller);
        void executeScript(long nativeTabImpl, String script, boolean useSeparateIsolate,
                Callback<String> callback);
    }
}
