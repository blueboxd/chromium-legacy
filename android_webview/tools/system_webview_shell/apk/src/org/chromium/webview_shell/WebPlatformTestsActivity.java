// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

/**
 * A main activity to handle WPT requests.
 *
 * This is currently implemented to support minimum viable implementation such that
 * multi-window and JavaScript are enabled by default.
 *
 * TODO(crbug.com/994939): It is currently implemented to support only a single child. Although not
 *                         explicitly stated, there may be some WPT tests that require more than one
 *                         children, which is not supported for now.
 */
public class WebPlatformTestsActivity extends Activity {
    private static final String TAG = "WPTActivity";

    /**
     * A callback for testing.
     */
    @VisibleForTesting
    public interface TestCallback {
        /** Called after child layout becomes visible. */
        void onChildLayoutVisible();
        /** Called after child layout becomes invisible. */
        void onChildLayoutInvisible();
    }

    private LayoutInflater mLayoutInflater;
    private RelativeLayout mRootLayout;
    private WebView mWebView;
    private WebView mChildWebView;
    private LinearLayout mChildLayout;
    private TestCallback mTestCallback;

    private class MultiWindowWebChromeClient extends WebChromeClient {
        @Override
        public boolean onCreateWindow(
                WebView webView, boolean isDialog, boolean isUserGesture, Message resultMsg) {
            removeAndDestroyWebView(mChildWebView);
            // Note that WebView is not inflated but created programmatically
            // such that it can be destroyed separately.
            mChildWebView = new WebView(WebPlatformTestsActivity.this);
            setUpWebSettings(mChildWebView.getSettings());
            mChildWebView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView childWebView, String url) {
                    // Once the view has loaded, display its title for debugging.
                    TextView childTitleText = mChildLayout.findViewById(R.id.childTitleText);
                    childTitleText.setText(childWebView.getTitle());
                }
            });
            mChildWebView.setWebChromeClient(new WebChromeClient() {
                @Override
                public void onCloseWindow(WebView childWebView) {
                    closeChild();
                }
            });
            // Add the new WebView to the layout
            mChildWebView.setLayoutParams(
                    new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.MATCH_PARENT));
            // Tell the transport about the new view
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(mChildWebView);
            resultMsg.sendToTarget();

            mChildLayout.addView(mChildWebView);
            // Make the child webview's layout visible
            mChildLayout.setVisibility(View.VISIBLE);
            if (mTestCallback != null) mTestCallback.onChildLayoutVisible();
            return true;
        }

        @Override
        public void onCloseWindow(WebView webView) {
            // Ignore window.close() on the test runner window.
        }
    }

    /** Remove and destroy a webview if it exists. */
    private void removeAndDestroyWebView(WebView webView) {
        if (webView == null) return;
        ViewGroup parent = (ViewGroup) webView.getParent();
        if (parent != null) parent.removeView(webView);
        webView.destroy();
    }

    private String getUrlFromIntent() {
        if (getIntent() == null) return null;
        return getIntent().getDataString();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        WebView.setWebContentsDebuggingEnabled(true);
        mLayoutInflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        setContentView(R.layout.activity_web_platform_tests);
        mRootLayout = findViewById(R.id.rootLayout);
        mWebView = findViewById(R.id.webview);
        mChildLayout = createChildLayout();

        String url = getUrlFromIntent();
        if (url == null) {
            // This is equivalent to Chrome's WPT setup.
            setUpMainWebView("about:blank");
        } else {
            Log.w(TAG,
                    "Handling a non-empty intent. This should only be used for testing. URL: "
                            + url);
            setUpMainWebView(url);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        removeAndDestroyWebView(mWebView);
        mWebView = null;
        removeAndDestroyWebView(mChildWebView);
        mChildWebView = null;
    }

    private LinearLayout createChildLayout() {
        // Provide parent such that MATCH_PARENT layout params can work. Ignore the return value
        // which is mRootLayout.
        mLayoutInflater.inflate(R.layout.activity_web_platform_tests_child, mRootLayout);
        // Choose what has just been added.
        LinearLayout childLayout =
                (LinearLayout) mRootLayout.getChildAt(mRootLayout.getChildCount() - 1);

        Button childCloseButton = childLayout.findViewById(R.id.childCloseButton);
        childCloseButton.setOnClickListener((View v) -> { closeChild(); });
        return childLayout;
    }

    private void setUpWebSettings(WebSettings settings) {
        // Required by WPT.
        settings.setJavaScriptEnabled(true);
        // Enable multi-window.
        settings.setJavaScriptCanOpenWindowsAutomatically(true);
        settings.setSupportMultipleWindows(true);
        // Respect "viewport" HTML meta tag. This is false by default, but set to false to be clear.
        settings.setUseWideViewPort(false);
    }

    private void setUpMainWebView(String url) {
        setUpWebSettings(mWebView.getSettings());
        mWebView.setWebChromeClient(new MultiWindowWebChromeClient());
        mWebView.loadUrl(url);
    }

    private void closeChild() {
        TextView childTitleText = mChildLayout.findViewById(R.id.childTitleText);
        childTitleText.setText("");
        mChildLayout.setVisibility(View.INVISIBLE);
        removeAndDestroyWebView(mChildWebView);
        mChildWebView = null;
        if (mTestCallback != null) mTestCallback.onChildLayoutInvisible();
    }

    @VisibleForTesting
    public void setTestCallback(TestCallback testDelegate) {
        mTestCallback = testDelegate;
    }

    @VisibleForTesting
    public WebView getTestRunnerWebView() {
        return mWebView;
    }
}
