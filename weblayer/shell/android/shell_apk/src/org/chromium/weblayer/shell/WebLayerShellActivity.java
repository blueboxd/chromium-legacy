// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.app.DownloadManager;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.BrowserFragment;
import org.chromium.weblayer.BrowserFragmentController;
import org.chromium.weblayer.BrowserObserver;
import org.chromium.weblayer.DownloadDelegate;
import org.chromium.weblayer.FullscreenDelegate;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.io.File;
import java.util.List;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends FragmentActivity {
    private static final String TAG = "WebLayerShell";
    private static final String KEY_MAIN_VIEW_ID = "mainViewId";

    private Profile mProfile;
    private BrowserFragmentController mBrowserFragmentController;
    private BrowserController mBrowserController;
    private EditText mUrlView;
    private ProgressBar mLoadProgressBar;
    private View mMainView;
    private int mMainViewId;
    private ViewGroup mTopContentsContainer;
    private BrowserFragment mFragment;

    public BrowserController getBrowserController() {
        return mBrowserController;
    }

    public BrowserFragmentController getBrowserFragmentController() {
        return mBrowserFragmentController;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout mainView = new LinearLayout(this);
        if (savedInstanceState == null) {
            mMainViewId = View.generateViewId();
        } else {
            mMainViewId = savedInstanceState.getInt(KEY_MAIN_VIEW_ID);
        }
        mainView.setId(mMainViewId);
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
        mTopContentsContainer = new RelativeLayout(this);
        mTopContentsContainer.addView(mUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        RelativeLayout.LayoutParams progressLayoutParams = new RelativeLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        progressLayoutParams.addRule(RelativeLayout.ALIGN_BOTTOM, mUrlView.getId());
        progressLayoutParams.setMargins(0, 0, 0, -10);
        mTopContentsContainer.addView(mLoadProgressBar, progressLayoutParams);

        try {
            // This ensures asynchronous initialization of WebLayer on first start of activity.
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened.
            WebLayer.create(getApplication()).addCallback(webLayer -> onWebLayerReady(
                    savedInstanceState));
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    private void onWebLayerReady(Bundle savedInstanceState) {
        if (isFinishing() || isDestroyed()) return;

        mFragment = getOrCreateBrowserFragment(savedInstanceState);
        mBrowserFragmentController = mFragment.getController();
        mBrowserFragmentController.getBrowserController().setFullscreenDelegate(
                new FullscreenDelegate() {
                    private int mSystemVisibilityToRestore;

                    @Override
                    public void enterFullscreen(Runnable exitFullscreenRunnable) {
                        // This comes from Chrome code to avoid an extra resize.
                        final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                        attrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                        getWindow().setAttributes(attrs);

                        View decorView = getWindow().getDecorView();
                        // Caching the system ui visibility is ok for shell, but likely not ok for
                        // real code.
                        mSystemVisibilityToRestore = decorView.getSystemUiVisibility();
                        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                                | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                                | View.SYSTEM_UI_FLAG_LOW_PROFILE
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
                    }

                    @Override
                    public void exitFullscreen() {
                        View decorView = getWindow().getDecorView();
                        decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

                        final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                        if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS)
                                != 0) {
                            attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                            getWindow().setAttributes(attrs);
                        }
                    }
                });
        mProfile = mBrowserFragmentController.getProfile();

        mBrowserFragmentController.setTopView(mTopContentsContainer);

        mBrowserController = mBrowserFragmentController.getBrowserController();
        String startupUrl = getUrlFromIntent(getIntent());
        if (TextUtils.isEmpty(startupUrl)) {
            startupUrl = "http://google.com";
        }
        loadUrl(startupUrl);
        mBrowserController.addObserver(new BrowserObserver() {
            @Override
            public void visibleUrlChanged(Uri uri) {
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
        mBrowserController.setDownloadDelegate(new DownloadDelegate() {
            @Override
            public void downloadRequested(String url, String userAgent, String contentDisposition,
                    String mimetype, long contentLength) {
                DownloadManager.Request request = new DownloadManager.Request(Uri.parse(url));
                request.setNotificationVisibility(
                        DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
                getSystemService(DownloadManager.class).enqueue(request);
            }
        });
    }

    private BrowserFragment getOrCreateBrowserFragment(Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return (BrowserFragment) fragments.get(0);
            }
        }

        File profile = new File(getFilesDir(), "defaultProfile");
        BrowserFragment fragment = WebLayer.createBrowserFragment(profile.getPath());
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(mMainViewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
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

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // When restoring Fragments, FragmentManager tries to put them in the containers with same
        // ids as before.
        outState.putInt(KEY_MAIN_VIEW_ID, mMainViewId);
    }

    @Override
    public void onBackPressed() {
        if (mBrowserFragmentController != null) {
            NavigationController controller = mBrowserFragmentController.getBrowserController()
                    .getNavigationController();
            if (controller.canGoBack()) {
                controller.goBack();
                return;
            }
        }
        super.onBackPressed();
    }
}
