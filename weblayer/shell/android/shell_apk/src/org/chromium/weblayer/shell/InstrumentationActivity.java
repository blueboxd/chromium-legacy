// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.util.List;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends FragmentActivity {
    private static final String TAG = "WLInstrumentation";
    private static final String KEY_MAIN_VIEW_ID = "mainViewId";

    public static final String EXTRA_PERSISTENCE_ID = "EXTRA_PERSISTENCE_ID";
    public static final String EXTRA_PROFILE_NAME = "EXTRA_PROFILE_NAME";

    // Used in tests to specify whether WebLayer should be created automatically on launch.
    // True by default. If set to false, the test should call loadWebLayerSync.
    public static final String EXTRA_CREATE_WEBLAYER = "EXTRA_CREATE_WEBLAYER";

    private Profile mProfile;
    private Fragment mFragment;
    private Browser mBrowser;
    private Tab mTab;
    private EditText mUrlView;
    private View mMainView;
    private int mMainViewId;
    private ViewGroup mTopContentsContainer;
    private IntentInterceptor mIntentInterceptor;
    private Bundle mSavedInstanceState;
    private TabCallback mTabCallback;

    public Tab getTab() {
        return mTab;
    }

    public Fragment getFragment() {
        return mFragment;
    }

    public Browser getBrowser() {
        return mBrowser;
    }

    /** Interface used to intercept intents for testing. */
    public static interface IntentInterceptor {
        void interceptIntent(Fragment fragment, Intent intent, int requestCode, Bundle options);
    }

    public void setIntentInterceptor(IntentInterceptor interceptor) {
        mIntentInterceptor = interceptor;
    }

    @Override
    public void startActivityFromFragment(
            Fragment fragment, Intent intent, int requestCode, Bundle options) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(fragment, intent, requestCode, options);
            return;
        }
        super.startActivityFromFragment(fragment, intent, requestCode, options);
    }

    public View getTopContentsContainer() {
        return mTopContentsContainer;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mSavedInstanceState = savedInstanceState;
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

        // The progress bar sits above the URL bar in Z order and at its bottom in Y.
        mTopContentsContainer = new RelativeLayout(this);
        mTopContentsContainer.addView(mUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        if (getIntent().getBooleanExtra(EXTRA_CREATE_WEBLAYER, true)) {
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened, so WebLayer#createAsync will
            // deliver WebLayer instance to callbacks immediately.
            createWebLayerAsync();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // When restoring Fragments, FragmentManager tries to put them in the containers with same
        // ids as before.
        outState.putInt(KEY_MAIN_VIEW_ID, mMainViewId);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mTabCallback != null) {
            mTab.unregisterTabCallback(mTabCallback);
            mTabCallback = null;
        }
    }

    private void createWebLayerAsync() {
        try {
            WebLayer.loadAsync(getApplicationContext(), webLayer -> onWebLayerReady());
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    public WebLayer loadWebLayerSync(Context appContext) {
        try {
            WebLayer webLayer = WebLayer.loadSync(appContext);
            onWebLayerReady();
            return webLayer;
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    private void onWebLayerReady() {
        if (mBrowser != null || isFinishing() || isDestroyed()) return;

        mFragment = getOrCreateBrowserFragment();
        mBrowser = Browser.fromFragment(mFragment);
        mProfile = mBrowser.getProfile();

        mBrowser.setTopView(mTopContentsContainer);

        if (mBrowser.getActiveTab() == null) {
            assert mBrowser.getTabs().size() == 0;
            // This happens with session restore enabled.
            mBrowser.registerTabListCallback(new TabListCallback() {
                @Override
                public void onTabAdded(Tab tab) {
                    if (mTab == null) {
                        mBrowser.unregisterTabListCallback(this);
                        setTab(tab);
                    }
                }
            });
        } else {
            setTab(mBrowser.getActiveTab());
        }
    }

    private void setTab(Tab tab) {
        assert mTab == null;
        mTab = tab;
        mTabCallback = new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        };
        mTab.registerTabCallback(mTabCallback);
    }

    private Fragment getOrCreateBrowserFragment() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (mSavedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return fragments.get(0);
            }
        }
        return createBrowserFragment(mMainViewId);
    }

    public Fragment createBrowserFragment(int viewId) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        String profileName = getIntent().hasExtra(EXTRA_PROFILE_NAME)
                ? getIntent().getStringExtra(EXTRA_PROFILE_NAME)
                : "DefaultProfile";
        String persistenceId = getIntent().hasExtra(EXTRA_PERSISTENCE_ID)
                ? getIntent().getStringExtra(EXTRA_PERSISTENCE_ID)
                : null;
        Fragment fragment = WebLayer.createBrowserFragment(profileName, persistenceId);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(viewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
    }

    public void loadUrl(String url) {
        mTab.getNavigationController().navigate(Uri.parse(url));
        mUrlView.clearFocus();
    }

    public void setRetainInstance(boolean retain) {
        mFragment.setRetainInstance(retain);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
