// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.demo;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserFragment;
import org.chromium.weblayer.BrowserFragmentController;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerAnimationDemoActivity extends FragmentActivity {
    private static final String TAG = "AnimationDemo";

    private static final boolean USE_WEBVIEW = false;

    private Profile mProfile;
    private final BrowserFragment mBrowserFragments[] = new BrowserFragment[4];
    private final ContainerFrameLayout mContainerViews[] = new ContainerFrameLayout[4];
    private final MyWebView mWebViews[] = new MyWebView[4];
    private FrameLayout mMainView;
    private WebLayer mWebLayer;

    public static class ContainerFrameLayout extends FrameLayout {
        private final BrowserFragmentController mFragmentController;
        private int mIndex;
        private boolean mInterceptTouchEvent;

        public ContainerFrameLayout(Context context, BrowserFragmentController fragmentController,
                int index) {
            super(context);
            mFragmentController = fragmentController;
            mIndex = index;
        }

        public BrowserFragmentController getFragmentController() {
            return mFragmentController;
        }

        public void setInterceptTouchEvent(boolean intercept) {
            mInterceptTouchEvent = intercept;
        }

        public int getIndex() {
            return mIndex;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return mInterceptTouchEvent;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!mInterceptTouchEvent) return super.onTouchEvent(event);
            if (event.getAction() == KeyEvent.ACTION_UP) {
                performClick();
            }
            return true;
        }
    }

    private static class MyWebView extends WebView {
        private int mIndex;
        private boolean mInterceptTouchEvent;

        public MyWebView(Context context, int index) {
            super(context);
            mIndex = index;
        }

        public void setInterceptTouchEvent(boolean intercept) {
            mInterceptTouchEvent = intercept;
        }

        public int getIndex() {
            return mIndex;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return mInterceptTouchEvent;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!mInterceptTouchEvent) return super.onTouchEvent(event);
            if (event.getAction() == KeyEvent.ACTION_UP) {
                performClick();
            }
            return true;
        }
    }

    private void createNewFragment(int index) {
        int viewId = View.generateViewId();

        mBrowserFragments[index] = WebLayer.createBrowserFragment(null);
        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, mBrowserFragments[index]);
        transaction.commitNow();

        final BrowserController controller = getFragmentController(index).getBrowserController();

        ContainerFrameLayout container =
                new ContainerFrameLayout(this, getFragmentController(index), index);
        mContainerViews[index] = container;
        container.setId(viewId);
        mMainView.addView(container);



        EditText urlView = new EditText(this);
        urlView.setSelectAllOnFocus(true);
        urlView.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        urlView.setImeOptions(EditorInfo.IME_ACTION_GO);
        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        urlView.setBackgroundColor(0xFFa9a9a9);
        urlView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO)
                        && (event == null || event.getKeyCode() != KeyEvent.KEYCODE_ENTER
                                || event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }
                String url = urlView.getText().toString();
                controller.getNavigationController().navigate(Uri.parse(sanitizeUrl(url)));
                return true;
            }
        });
        getFragmentController(index).setTopView(urlView);

        controller.addObserver(new BrowserObserver() {
            @Override
            public void visibleUrlChanged(Uri uri) {
                urlView.setText(uri.toString());
            }
        });
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        if (USE_WEBVIEW) {
            super.onCreate(savedInstanceState);

            FrameLayout mainView = new FrameLayout(this);
            mMainView = mainView;
            setContentView(mainView);

            for (int i = 0; i < 3; ++i) {
                mWebViews[i] = new MyWebView(this, i);
                mWebViews[i].setWebViewClient(new WebViewClient() {
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                        return false;
                    }
                });
                mMainView.addView(mWebViews[i]);
                WebSettings settings = mWebViews[i].getSettings();
                settings.setJavaScriptEnabled(true);

                // Default layout behavior for chrome on android.
                settings.setUseWideViewPort(true);
                settings.setLoadWithOverviewMode(true);
                settings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.TEXT_AUTOSIZING);
            }
        } else {
            // Only call init for main process.
            try {
                mWebLayer = WebLayer.create(getApplication()).get();
            } catch (Exception e) {
                throw new RuntimeException("failed loading WebLayer", e);
            }
            if (savedInstanceState != null) {
                // This prevents FragmentManager from restoring fragments.
                // TODO(pshmakov): restore fragments properly, as in WebLayerShellActivity.
                savedInstanceState.remove("android:support:fragments");
            }

            super.onCreate(savedInstanceState);

            FrameLayout mainView = new FrameLayout(this);
            mMainView = mainView;
            setContentView(mainView);

            createNewFragment(0);
            createNewFragment(1);
            createNewFragment(2);
            mProfile = getFragmentController(0).getProfile();

            getNavigationController(0).navigate(Uri.parse(sanitizeUrl("https://www.google.com")));
            getNavigationController(1).navigate(Uri.parse(sanitizeUrl("https://en.wikipedia.org")));
            getNavigationController(2).navigate(Uri.parse(sanitizeUrl("https://www.chromium.org")));
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (USE_WEBVIEW) {
            mWebViews[0].post(() -> {
                mWebViews[0].loadUrl("https://www.google.com");
                mWebViews[1].loadUrl("https://en.wikipedia.org");
                mWebViews[2].loadUrl("https://www.chromium.org");
                for (int i = 0; i < 3; ++i) {
                    mWebViews[i].setOnClickListener(new OnClickImpl());
                    animateDown(mWebViews[i]);
                }
            });
        } else {
            mContainerViews[0].setOnClickListener(new OnClickImpl());
            mContainerViews[1].setOnClickListener(new OnClickImpl());
            mContainerViews[2].setOnClickListener(new OnClickImpl());
            mContainerViews[0].post(() -> {
                getFragmentController(0).setSupportsEmbedding(true).addCallback(
                        (Boolean result) -> animateDown(mContainerViews[0]));
                getFragmentController(1).setSupportsEmbedding(true).addCallback(
                        (Boolean result) -> animateDown(mContainerViews[1]));
                getFragmentController(2).setSupportsEmbedding(true).addCallback(
                        (Boolean result) -> animateDown(mContainerViews[2]));
            });
        }
    }

    private BrowserFragmentController getFragmentController(int index) {
        return mBrowserFragments[index].getController();
    }

    private NavigationController getNavigationController(int index) {
        return getFragmentController(index).getBrowserController().getNavigationController();
    }

    private ContainerFrameLayout mFullscreenContainer;
    private MyWebView mFullscreenWebView;
    public class OnClickImpl implements View.OnClickListener {
        @Override
        public void onClick(View v) {
            if (USE_WEBVIEW) {
                MyWebView webview = (MyWebView) v;
                if (mFullscreenWebView == webview) return;

                mMainView.removeView(webview);
                mMainView.addView(webview, 0);

                if (mFullscreenWebView != null) {
                    animateDown(mFullscreenWebView);
                }
                animateUp(webview);
                mFullscreenWebView = webview;
            } else {
                ContainerFrameLayout container = (ContainerFrameLayout) v;
                if (mFullscreenContainer == container) return;

                // Move to back by bringing others to the front.
                // This avoids detaching and reattaching any views.
                for (int i = 0; i < 3; ++i) {
                    if (mContainerViews[i] == container) continue;
                    mMainView.bringChildToFront(mContainerViews[i]);
                }

                if (mFullscreenContainer != null) {
                    animateDown(mFullscreenContainer);
                }
                animateUp(container);
                mFullscreenContainer = container;
            }
        }
    }

    private static void animateDown(ContainerFrameLayout container) {
        // Start animation after fullying switched from SurfaceView to TextureView.
        int index = container.getIndex();
        container.getFragmentController().setSupportsEmbedding(true)
                .addCallback((Boolean result) -> {
                    container.animate()
                            .scaleX(1.0f / 3)
                            .scaleY(1.0f / 3)
                            .translationX(-container.getWidth() / 3.0f
                                    + (container.getWidth() / 3.0f * index))
                            .translationY(container.getHeight() / 3.0f)
                            .alpha(0.8f)
                            .setDuration(500);
                    container.setInterceptTouchEvent(true);
                });
    }

    private static void animateUp(ContainerFrameLayout container) {
        container.animate()
                .scaleX(1.0f)
                .scaleY(1.0f)
                .translationX(0)
                .translationY(0)
                .alpha(1.0f)
                .setDuration(500)
                .withEndAction(() -> {
                    container.getFragmentController().setSupportsEmbedding(false);
                    container.setInterceptTouchEvent(false);
                });
    }

    private static void animateDown(MyWebView webview) {
        int index = webview.getIndex();
        webview.animate()
                .scaleX(1.0f / 3)
                .scaleY(1.0f / 3)
                .translationX(-webview.getWidth() / 3.0f + (webview.getWidth() / 3.0f * index))
                .translationY(webview.getHeight() / 3.0f)
                .alpha(0.8f)
                .setDuration(500);
        webview.setInterceptTouchEvent(true);
    }

    private static void animateUp(MyWebView webview) {
        webview.animate()
                .scaleX(1.0f)
                .scaleY(1.0f)
                .translationX(0)
                .translationY(0)
                .alpha(1.0f)
                .setDuration(500);
        webview.setInterceptTouchEvent(false);
    }

    /**
     * Given an URL, this performs minimal sanitizing to ensure it will be valid.
     * @param url The url to be sanitized.
     * @return The sanitized URL.
     */
    public static String sanitizeUrl(String url) {
        if (url == null) return null;
        if (url.startsWith("www.") || url.indexOf(":") == -1) url = "http://" + url;
        return url;
    }
}
