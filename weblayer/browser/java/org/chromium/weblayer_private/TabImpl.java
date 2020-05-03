// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Build;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.AndroidRuntimeException;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill.AutofillActionModeCallback;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderImpl;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.find_in_page.FindInPageBridge;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindResultBar;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Implementation of ITab.
 */
@JNINamespace("weblayer")
public final class TabImpl extends ITab.Stub {
    private static int sNextId = 1;
    // Map from id to TabImpl.
    private static final Map<Integer, TabImpl> sTabMap = new HashMap<Integer, TabImpl>();
    private long mNativeTab;

    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private TabCallbackProxy mTabCallbackProxy;
    private NavigationControllerImpl mNavigationController;
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
    private MediaStreamManager mMediaStreamManager;
    private NewTabCallbackProxy mNewTabCallbackProxy;
    private ITabClient mClient;
    private final int mId;

    // A list of browser control visibility constraints, indexed by ImplControlsVisibilityReason.
    private List<BrowserControlsVisibilityDelegate> mBrowserControlsDelegates;
    // Computes a net browser control visibility constraint from constituent constraints.
    private ComposedBrowserControlsVisibilityDelegate mBrowserControlsVisibility;
    // Invoked when the computed visibility constraint changes.
    private Callback<Integer> mConstraintsUpdatedCallback;

    private IFindInPageCallbackClient mFindInPageCallbackClient;
    private FindInPageBridge mFindInPageBridge;
    private FindResultBar mFindResultBar;
    // See usage note in {@link #onFindResultAvailable}.
    boolean mWaitingForMatchRects;
    private InterceptNavigationDelegateClientImpl mInterceptNavigationDelegateClient;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

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

    public static TabImpl getTabById(int tabId) {
        return sTabMap.get(tabId);
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
        mWebContents = TabImplJni.get().getWebContents(mNativeTab);
        mViewAndroidDelegate = new ViewAndroidDelegate(null) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY,
                    int topControlsMinHeightOffsetY) {
                BrowserViewController viewController = getViewController();
                if (viewController != null) {
                    viewController.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
                }
            }
            @Override
            public void onBottomControlsChanged(
                    int bottomControlsOffsetY, int bottomControlsMinHeightOffsetY) {
                BrowserViewController viewController = getViewController();
                if (viewController != null) {
                    viewController.onBottomControlsChanged(bottomControlsOffsetY);
                }
            }
        };
        mWebContents.initialize("", mViewAndroidDelegate, new InternalAccessDelegateImpl(),
                windowAndroid, WebContents.createDefaultInternalsHolder());

        mWebContentsObserver = new WebContentsObserver() {
            @Override
            public void didStartNavigation(NavigationHandle navigationHandle) {
                if (navigationHandle.isInMainFrame() && !navigationHandle.isSameDocument()) {
                    hideFindInPageUiAndNotifyClient();
                }
            }
        };
        mWebContents.addObserver(mWebContentsObserver);

        mMediaStreamManager = new MediaStreamManager(this);

        mBrowserControlsDelegates = new ArrayList<BrowserControlsVisibilityDelegate>();
        mBrowserControlsVisibility = new ComposedBrowserControlsVisibilityDelegate();
        for (int i = 0; i < ImplControlsVisibilityReason.REASON_COUNT; ++i) {
            BrowserControlsVisibilityDelegate delegate =
                    new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH);
            mBrowserControlsDelegates.add(delegate);
            mBrowserControlsVisibility.addDelegate(delegate);
        }
        mConstraintsUpdatedCallback = (constraints) -> onBrowserControlsStateUpdated(constraints);
        mBrowserControlsVisibility.addObserver(mConstraintsUpdatedCallback);

        mInterceptNavigationDelegateClient = new InterceptNavigationDelegateClientImpl(this);
        mInterceptNavigationDelegate =
                new InterceptNavigationDelegateImpl(mInterceptNavigationDelegateClient);
        mInterceptNavigationDelegateClient.initializeWithDelegate(mInterceptNavigationDelegate);
        sTabMap.put(mId, this);
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
        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(mWebContents);
        controller.setActionModeCallback(new ActionModeCallback(mWebContents));
    }

    public void updateFromBrowser() {
        mWebContents.setTopLevelNativeWindow(mBrowser.getWindowAndroid());
        mViewAndroidDelegate.setContainerView(mBrowser.getViewAndroidDelegateContainerView());
        updateWebContentsVisibility();

        boolean attached = (mBrowser.getContext() != null);
        mInterceptNavigationDelegateClient.onActivityAttachmentChanged(attached);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            SelectionPopupController selectionController =
                    SelectionPopupController.fromWebContents(mWebContents);
            if (mBrowser.getContext() == null) {
                // The Context and ViewContainer in which Autofill was previously operating have
                // gone away, so tear down |mAutofillProvider|.
                mAutofillProvider = null;
                selectionController.setNonSelectionActionModeCallback(null);
            } else {
                // Set up |mAutofillProvider| to operate in the new Context.
                mAutofillProvider = new AutofillProviderImpl(mBrowser.getContext(),
                        mBrowser.getViewAndroidDelegateContainerView(), "WebLayer");
                mAutofillProvider.setWebContents(mWebContents);

                selectionController.setNonSelectionActionModeCallback(
                        new AutofillActionModeCallback(mBrowser.getContext(), mAutofillProvider));
            }

            TabImplJni.get().onAutofillProviderChanged(mNativeTab, mAutofillProvider);
        }
    }

    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {
        if (mAutofillProvider == null) return;
        mAutofillProvider.onProvideAutoFillVirtualStructure(structure, flags);
    }

    public void autofill(final SparseArray<AutofillValue> values) {
        if (mAutofillProvider == null) return;
        mAutofillProvider.autofill(values);
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
    public void onDidGainActive(
            long topControlsContainerViewHandle, long bottomControlsContainerViewHandle) {
        // attachToFragment() must be called before activate().
        assert mBrowser != null;
        TabImplJni.get().setBrowserControlsContainerViews(
                mNativeTab, topControlsContainerViewHandle, bottomControlsContainerViewHandle);
        updateWebContentsVisibility();
        mWebContents.onShow();
    }

    /**
     * Called when this TabImpl is no longer the active TabImpl.
     */
    public void onDidLoseActive() {
        hideFindInPageUiAndNotifyClient();
        mWebContents.onHide();
        updateWebContentsVisibility();
        TabImplJni.get().setBrowserControlsContainerViews(mNativeTab, 0, 0);
    }

    /**
     * Returns whether this Tab is visible.
     */
    public boolean isVisible() {
        return (mBrowser.getActiveTab() == this && mBrowser.isStarted());
    }

    private void updateWebContentsVisibility() {
        boolean visibleNow = isVisible();
        boolean webContentsVisible = mWebContents.getVisibility() == Visibility.VISIBLE;
        if (visibleNow) {
            if (!webContentsVisible) mWebContents.onShow();
        } else {
            if (webContentsVisible) mWebContents.onHide();
        }
    }

    public void loadUrl(LoadUrlParams loadUrlParams) {
        String url = loadUrlParams.getUrl();
        if (url == null || url.isEmpty()) return;

        GURL fixedUrl = UrlFormatter.fixupUrl(url);
        if (!fixedUrl.isValid()) return;

        loadUrlParams.setUrl(fixedUrl.getSpec());
        getWebContents().getNavigationController().loadUrl(loadUrlParams);
    }

    public WebContents getWebContents() {
        return mWebContents;
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
        mProfile.setDownloadCallbackClient(client);
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

        // Refuse to start a find session when the browser controls are forced hidden.
        if (mBrowserControlsVisibility.get() == BrowserControlsState.HIDDEN) return false;

        setBrowserControlsVisibilityConstraint(
                ImplControlsVisibilityReason.FIND_IN_PAGE, BrowserControlsState.SHOWN);

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

        setBrowserControlsVisibilityConstraint(
                ImplControlsVisibilityReason.FIND_IN_PAGE, BrowserControlsState.BOTH);

        try {
            if (mFindInPageCallbackClient != null) mFindInPageCallbackClient.onFindEnded();
            mFindInPageCallbackClient = null;
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    @Override
    public void dispatchBeforeUnloadAndClose() {
        StrictModeWorkaround.apply();
        mWebContents.dispatchBeforeUnload(false);
    }

    @Override
    public boolean dismissTransientUi() {
        BrowserViewController viewController = getViewController();
        if (viewController != null && viewController.dismissTabModalOverlay()) return true;

        if (mWebContents.isFullscreenForCurrentTab()) {
            mWebContents.exitFullscreen();
            return true;
        }

        SelectionPopupController popup = SelectionPopupController.fromWebContents(mWebContents);
        if (popup != null && popup.isSelectActionBarShowing()) {
            popup.clearSelection();
            return true;
        }

        return false;
    }

    @Override
    public String getGuid() {
        return TabImplJni.get().getGuid(mNativeTab);
    }

    @Override
    public void captureScreenShot(float scale, IObjectWrapper valueCallback) {
        StrictModeWorkaround.apply();
        ValueCallback<Pair<Bitmap, Integer>> unwrappedCallback =
                (ValueCallback<Pair<Bitmap, Integer>>) ObjectWrapper.unwrap(
                        valueCallback, ValueCallback.class);
        TabImplJni.get().captureScreenShot(mNativeTab, scale, unwrappedCallback);
    }

    @CalledByNative
    private static void runCaptureScreenShotCallback(
            ValueCallback<Pair<Bitmap, Integer>> callback, Bitmap bitmap, int errorCode) {
        callback.onReceiveValue(Pair.create(bitmap, errorCode));
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

    @Override
    public void setMediaCaptureCallbackClient(IMediaCaptureCallbackClient client) {
        mMediaStreamManager.setClient(client);
    }

    @Override
    public void stopMediaCapturing() {
        mMediaStreamManager.stopStreaming();
    }

    @CalledByNative
    private void handleCloseFromWebContents() throws RemoteException {
        // On clients < 84 WebContents-initiated tab closing was delegated to the client; this flow
        // should not be used, as the client will not be expecting it.
        assert WebLayerFactoryImpl.getClientMajorVersion() >= 84;

        if (getBrowser() == null) return;
        getBrowser().destroyTab(this);
    }

    public void destroy() {
        // Ensure that this method isn't called twice.
        assert mInterceptNavigationDelegate != null;

        if (WebLayerFactoryImpl.getClientMajorVersion() >= 84) {
            // Notify the client that this instance is being destroyed to prevent it from calling
            // back into this object if the embedder mistakenly tries to do so.
            try {
                mClient.onTabDestroyed();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        if (mTabCallbackProxy != null) {
            mTabCallbackProxy.destroy();
            mTabCallbackProxy = null;
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

        mInterceptNavigationDelegateClient.destroy();
        mInterceptNavigationDelegate = null;

        mMediaStreamManager.destroy();
        mMediaStreamManager = null;

        sTabMap.remove(mId);

        // ObservableSupplierImpl.addObserver() posts a task to notify the observer, ensure the
        // callback isn't run after destroy() is called (otherwise we'll get crashes as the native
        // tab has been deleted).
        mBrowserControlsVisibility.removeObserver(mConstraintsUpdatedCallback);
        hideFindInPageUiAndNotifyClient();
        mFindInPageCallbackClient = null;
        mNavigationController = null;
        mWebContents.removeObserver(mWebContentsObserver);
        TabImplJni.get().deleteTab(mNativeTab);
        mNativeTab = 0;
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        BrowserViewController viewController = getViewController();
        return viewController != null && viewController.doBrowserControlsShrinkRendererSize();
    }

    @CalledByNative
    public void setBrowserControlsVisibilityConstraint(
            @ImplControlsVisibilityReason int reason, @BrowserControlsState int constraint) {
        mBrowserControlsDelegates.get(reason).set(constraint);
    }

    @CalledByNative
    public void showRepostFormWarningDialog() {
        BrowserViewController viewController = getViewController();
        if (viewController == null) {
            mWebContents.getNavigationController().cancelPendingReload();
        } else {
            viewController.showRepostFormWarningDialog();
        }
    }

    private static String nonEmptyOrNull(String s) {
        return TextUtils.isEmpty(s) ? null : s;
    }

    @CalledByNative
    private void showContextMenu(ContextMenuParams params) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 82) return;
        mClient.showContextMenu(ObjectWrapper.wrap(params.getPageUrl()),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkUrl())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getTitleText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getSrcUrl())));
    }

    private void onBrowserControlsStateUpdated(int state) {
        TabImplJni.get().updateBrowserControlsState(mNativeTab, state);
        // If something has overridden the FIP's SHOWN constraint, cancel FIP. This causes FIP to
        // dismiss when entering fullscreen.
        if (state != BrowserControlsState.SHOWN) {
            hideFindInPageUiAndNotifyClient();
        }
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
        void setBrowserControlsContainerViews(long nativeTabImpl,
                long nativeTopBrowserControlsContainerView,
                long nativeBottomBrowserControlsContainerView);
        void deleteTab(long tab);
        WebContents getWebContents(long nativeTabImpl);
        void executeScript(long nativeTabImpl, String script, boolean useSeparateIsolate,
                Callback<String> callback);
        void updateBrowserControlsState(long nativeTabImpl, int newConstraint);
        String getGuid(long nativeTabImpl);
        void captureScreenShot(long nativeTabImpl, float scale,
                ValueCallback<Pair<Bitmap, Integer>> valueCallback);
    }
}
