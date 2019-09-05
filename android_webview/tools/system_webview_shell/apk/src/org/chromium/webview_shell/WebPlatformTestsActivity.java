// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.os.Message;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.VisibleForTesting;

/**
 * A main activity to handle WPT requests.
 *
 * This is currently implemented to support minimum viable implementation such that
 * Multi-window and JavaScript are enabled by default.
 *
 * TODO(crbug.com/994939): It is currently implemented to support only a single child. Although not
 *                         explicitly stated, there may be some WPT tests that require more than one
 *                         children, which is not supported for now.
 */
public class WebPlatformTestsActivity extends Activity {
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

    private WebView mWebView;
    private WebView mChildWebView;
    private RelativeLayout mChildLayout;
    private RelativeLayout mBrowserLayout;
    private Button mChildCloseButton;
    private TextView mChildTitleText;
    private TestCallback mTestCallback;

    private class MultiWindowWebChromeClient extends WebChromeClient {
        @Override
        public boolean onCreateWindow(
                WebView webView, boolean isDialog, boolean isUserGesture, Message resultMsg) {
            removeAndDestroyChildWebView();
            // Now create a new WebView
            mChildWebView = new WebView(WebPlatformTestsActivity.this);
            WebSettings settings = mChildWebView.getSettings();
            setUpWebSettings(settings);
            settings.setUseWideViewPort(false);
            mChildWebView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView childWebView, String url) {
                    // Once the view has loaded, display its title for debugging.
                    mChildTitleText.setText(childWebView.getTitle());
                }
            });
            mChildWebView.setWebChromeClient(new WebChromeClient() {
                @Override
                public void onCloseWindow(WebView childWebView) {
                    closeChild();
                }
            });
            // Add the new WebView to the layout
            mChildWebView.setLayoutParams(new RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            // Tell the transport about the new view
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(mChildWebView);
            resultMsg.sendToTarget();

            mBrowserLayout.addView(mChildWebView);
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

    /** Remove and destroy a child webview if it exists. */
    private void removeAndDestroyChildWebView() {
        if (mChildWebView == null) return;
        ViewGroup parent = (ViewGroup) mChildWebView.getParent();
        if (parent != null) parent.removeView(mChildWebView);
        mChildWebView.destroy();
        mChildWebView = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        WebView.setWebContentsDebuggingEnabled(true);
        setContentView(R.layout.activity_web_platform_tests);
        setUpWidgets();
        // This is equivalent to Chrome's WPT setup.
        setUpWebView("about:blank");
    }

    private void setUpWidgets() {
        mBrowserLayout = findViewById(R.id.mainBrowserLayout);
        mChildLayout = findViewById(R.id.childLayout);
        mChildTitleText = findViewById(R.id.childTitleText);
        mChildCloseButton = findViewById(R.id.childCloseButton);
        mChildCloseButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                switch (view.getId()) {
                    case R.id.childCloseButton:
                        closeChild();
                        break;
                }
            }
        });
    }

    private void setUpWebSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);
        // Enable multi-window.
        settings.setJavaScriptCanOpenWindowsAutomatically(true);
        settings.setSupportMultipleWindows(true);
    }

    private void setUpWebView(String url) {
        mWebView = findViewById(R.id.webview);
        setUpWebSettings(mWebView.getSettings());

        mWebView.setWebChromeClient(new MultiWindowWebChromeClient());

        mWebView.loadUrl(url);
    }

    private void closeChild() {
        mChildTitleText.setText("");
        mChildLayout.setVisibility(View.INVISIBLE);
        removeAndDestroyChildWebView();
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
