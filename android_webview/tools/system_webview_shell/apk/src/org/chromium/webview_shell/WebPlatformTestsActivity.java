// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.os.Message;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.TextView;

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
    private WebView mWebView;
    private RelativeLayout mChildLayout;
    private RelativeLayout mBrowserLayout;
    private Button mChildCloseButton;
    private TextView mChildTitleText;

    private class MultiWindowWebChromeClient extends WebChromeClient {
        @Override
        public boolean onCreateWindow(
                WebView view, boolean isDialog, boolean isUserGesture, Message resultMsg) {
            // Make the child webview's layout visible
            mChildLayout.setVisibility(View.VISIBLE);

            // Now create a new WebView
            WebView newView = new WebView(WebPlatformTestsActivity.this);
            WebSettings settings = newView.getSettings();
            setUpWebSettings(settings);
            settings.setUseWideViewPort(false);
            newView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView view, String url) {
                    // Once the view has loaded, display its title for debugging.
                    mChildTitleText.setText(view.getTitle());
                }
            });
            // Add the new WebView to the layout
            newView.setLayoutParams(new RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            mBrowserLayout.addView(newView);
            // Tell the transport about the new view
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(newView);
            resultMsg.sendToTarget();

            // Slide the new WebView up into view
            Animation slideUp =
                    AnimationUtils.loadAnimation(WebPlatformTestsActivity.this, R.anim.slide_up);
            mChildLayout.startAnimation(slideUp);
            return true;
        }

        @Override
        public void onCloseWindow(WebView webView) {
            mBrowserLayout.removeView(webView);
            webView.destroy();
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
        Animation slideDown = AnimationUtils.loadAnimation(this, R.anim.slide_down);
        mChildLayout.startAnimation(slideDown);
        slideDown.setAnimationListener(new Animation.AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) {}

            @Override
            public void onAnimationEnd(Animation animation) {
                mChildTitleText.setText("");
                mChildLayout.setVisibility(View.INVISIBLE);
            }

            @Override
            public void onAnimationRepeat(Animation animation) {}
        });
    }
}
