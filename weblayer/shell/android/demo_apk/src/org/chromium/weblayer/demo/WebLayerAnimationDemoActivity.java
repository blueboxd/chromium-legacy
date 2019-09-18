// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.demo;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserFragmentImpl;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerAnimationDemoActivity extends FragmentActivity {
    private static final String TAG = "AnimationDemo";

    private static final boolean USE_WEBVIEW = false;

    private Profile mProfile;
    private final BrowserFragmentImpl mBrowserFragments[] = new BrowserFragmentImpl[4];
    private final ContainerFrameLayout mContainerViews[] = new ContainerFrameLayout[4];
    private final MyWebView mWebViews[] = new MyWebView[4];
    private FrameLayout mMainView;

    public static class ShellFragment extends Fragment {
        private BrowserFragmentImpl mBrowserFragment;

        ShellFragment(BrowserFragmentImpl browserFragment) {
            mBrowserFragment = browserFragment;
        }

        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            return mBrowserFragment.onCreateView();
        }
    }

    public static class ContainerFrameLayout extends FrameLayout {
        private int mIndex;
        private boolean mInterceptTouchEvent;

        public ContainerFrameLayout(Context context, int index) {
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
        ContainerFrameLayout container = new ContainerFrameLayout(this, index);
        mContainerViews[index] = container;
        int viewId = View.generateViewId();
        container.setId(viewId);
        mMainView.addView(container);

        mBrowserFragments[index] = mProfile.createBrowserFragment(this);
        final BrowserController controller = mBrowserFragments[index].getBrowserController();

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, new ShellFragment(mBrowserFragments[index]));
        transaction.commit();

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
        mBrowserFragments[index].setTopView(urlView);

        controller.addObserver(new BrowserObserver() {
            @Override
            public void displayURLChanged(Uri uri) {
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
            WebLayer.getInstance().init(getApplication());

            super.onCreate(savedInstanceState);

            FrameLayout mainView = new FrameLayout(this);
            mMainView = mainView;
            setContentView(mainView);

            mProfile = WebLayer.getInstance().createProfile(null);

            createNewFragment(0);
            createNewFragment(1);
            createNewFragment(2);

            mBrowserFragments[0].getBrowserController().getNavigationController().navigate(
                    Uri.parse(sanitizeUrl("https://www.google.com")));
            mBrowserFragments[1].getBrowserController().getNavigationController().navigate(
                    Uri.parse(sanitizeUrl("https://en.wikipedia.org")));
            mBrowserFragments[2].getBrowserController().getNavigationController().navigate(
                    Uri.parse(sanitizeUrl("https://www.chromium.org")));
        }
    }

    @Override
    protected void onDestroy() {
        for (int i = 0; i < mBrowserFragments.length; ++i) {
            BrowserFragmentImpl fragment = mBrowserFragments[i];
            if (fragment != null) fragment.destroy();
            mBrowserFragments[i] = null;
        }
        if (mProfile != null) mProfile.destroy();
        super.onDestroy();
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
                mBrowserFragments[0].setSupportsEmbedding(true);
                mBrowserFragments[1].setSupportsEmbedding(true);
                mBrowserFragments[2].setSupportsEmbedding(true);
                animateDown(mContainerViews[0]);
                animateDown(mContainerViews[1]);
                animateDown(mContainerViews[2]);
            });
        }
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

                mMainView.removeView(container);
                mMainView.addView(container, 0);

                if (mFullscreenContainer != null) {
                    animateDown(mFullscreenContainer);
                }
                animateUp(container);
                mFullscreenContainer = container;
            }
        }
    }

    private static void animateDown(ContainerFrameLayout container) {
        int index = container.getIndex();
        container.animate()
                .scaleX(1.0f / 3)
                .scaleY(1.0f / 3)
                .translationX(-container.getWidth() / 3.0f + (container.getWidth() / 3.0f * index))
                .translationY(container.getHeight() / 3.0f)
                .alpha(0.8f)
                .setDuration(500);
        container.setInterceptTouchEvent(true);
    }

    private static void animateUp(ContainerFrameLayout container) {
        container.animate()
                .scaleX(1.0f)
                .scaleY(1.0f)
                .translationX(0)
                .translationY(0)
                .alpha(1.0f)
                .setDuration(500);
        container.setInterceptTouchEvent(false);
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
