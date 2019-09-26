// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserFragmentController;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends FragmentActivity {
    private static final String TAG = "WebLayerShell";

    private Profile mProfile;
    private BrowserFragmentController mBrowserFragmentController;
    private BrowserController mBrowserController;
    private EditText mUrlView;
    private ProgressBar mLoadProgressBar;
    private View mMainView;

    public BrowserController getBrowserController() {
        return mBrowserController;
    }

    public BrowserFragmentController getBrowserFragmentController() {
        return mBrowserFragmentController;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Only call init for main process.
        WebLayer.getInstance().init(getApplication());

        LinearLayout mainView = new LinearLayout(this);
        int viewId = View.generateViewId();
        mainView.setId(viewId);
        mMainView = mainView;
        setContentView(mainView);

        mUrlView = new EditText(this);
        mUrlView.setId(View.generateViewId());
        mUrlView.setSelectAllOnFocus(true);
        mUrlView.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        mUrlView.setImeOptions(EditorInfo.IME_ACTION_GO);
        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        mUrlView.setBackgroundColor(0xFFa9a9a9);
        mUrlView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO)
                        && (event == null || event.getKeyCode() != KeyEvent.KEYCODE_ENTER
                                || event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }
                loadUrl(mUrlView.getText().toString());
                return true;
            }
        });

        mLoadProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mLoadProgressBar.setIndeterminate(false);
        mLoadProgressBar.setMax(100);
        mLoadProgressBar.setVisibility(View.INVISIBLE);

        // The progress bar sits above the URL bar in Z order and at its bottom in Y.
        RelativeLayout topContentsContainer = new RelativeLayout(this);
        topContentsContainer.addView(mUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        RelativeLayout.LayoutParams progressLayoutParams = new RelativeLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        progressLayoutParams.addRule(RelativeLayout.ALIGN_BOTTOM, mUrlView.getId());
        progressLayoutParams.setMargins(0, 0, 0, -10);
        topContentsContainer.addView(mLoadProgressBar, progressLayoutParams);

        mProfile = WebLayer.getInstance().createProfile(null);

        mBrowserFragmentController = mProfile.createBrowserFragmentController(this);

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, mBrowserFragmentController.getFragment());
        transaction.commit();

        mBrowserFragmentController.setTopView(topContentsContainer);

        mBrowserController = mBrowserFragmentController.getBrowserController();
        String startupUrl = getUrlFromIntent(getIntent());
        if (TextUtils.isEmpty(startupUrl)) {
            startupUrl = "http://google.com";
        }
        loadUrl(startupUrl);
        mBrowserController.addObserver(new BrowserObserver() {
            @Override
            public void displayURLChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }

            @Override
            public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {
                mLoadProgressBar.setVisibility(
                        isLoading && toDifferentDocument ? View.VISIBLE : View.INVISIBLE);
            }

            @Override
            public void loadProgressChanged(double progress) {
                mLoadProgressBar.setProgress((int) Math.round(100 * progress));
            }
        });
    }

    @Override
    protected void onDestroy() {
        if (mProfile != null) mProfile.destroy();
        if (mBrowserFragmentController != null) mBrowserFragmentController.destroy();
        super.onDestroy();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    public void loadUrl(String url) {
        mBrowserController.getNavigationController().navigate(Uri.parse(sanitizeUrl(url)));
        mUrlView.clearFocus();
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
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
