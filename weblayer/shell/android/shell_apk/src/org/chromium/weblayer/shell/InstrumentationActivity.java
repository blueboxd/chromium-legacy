// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.os.StrictMode.VmPolicy;
import android.text.InputType;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.ContextUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.NewTabType;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.util.ArrayList;
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
    private TabListCallback mTabListCallback;
    private List<Tab> mPreviousTabList = new ArrayList<>();

    private static boolean isJaCoCoEnabled() {
        // Nothing is set at runtime indicating jacoco is being used. This looks for the existence
        // of a javacoco class to determine if jacoco is enabled.
        try {
            Class.forName("org.jacoco.agent.rt.RT");
            return true;
        } catch (LinkageError | ClassNotFoundException e) {
        }
        return false;
    }

    public Tab getTab() {
        return mTab;
    }

    public Fragment getFragment() {
        return mFragment;
    }

    public Browser getBrowser() {
        return mBrowser;
    }

    /**
     * Explicitly destroys the fragment. There is normally no need to call this. It's useful for
     * tests that want to verify destruction.
     */
    public void destroyFragment() {
        removeCallbacks();

        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.remove(mFragment);
        transaction.commitNow();
        mFragment = null;
        mBrowser = null;
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

    @Override
    public void startActivity(Intent intent) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(null, intent, 0, null);
            return;
        }
        super.startActivity(intent);
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, int requestCode) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(null, intent, requestCode, null);
            return true;
        }
        return super.startActivityIfNeeded(intent, requestCode);
    }

    public View getTopContentsContainer() {
        return mTopContentsContainer;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        // JaCoCo injects code that does file access, which doesn't work well with strict mode.
        if (!isJaCoCoEnabled()) {
            StrictMode.setThreadPolicy(
                    new ThreadPolicy.Builder().detectAll().penaltyLog().penaltyDeath().build());
            // This doesn't use detectAll() as the untagged sockets policy is encountered in tests
            // using TestServer.
            StrictMode.setVmPolicy(new VmPolicy.Builder()
                                           .detectLeakedSqlLiteObjects()
                                           .detectLeakedClosableObjects()
                                           .penaltyLog()
                                           .penaltyDeath()
                                           .build());
        }
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
        removeCallbacks();
    }

    private void removeCallbacks() {
        if (mTabCallback != null) {
            mTab.unregisterTabCallback(mTabCallback);
            mTabCallback = null;
        }
        if (mTabListCallback != null) {
            mBrowser.unregisterTabListCallback(mTabListCallback);
            mTabListCallback = null;
        }
    }

    private void createWebLayerAsync() {
        try {
            // Get the Context from ContextUtils so tests get the wrapped version.
            WebLayer.loadAsync(ContextUtils.getApplicationContext(), webLayer -> onWebLayerReady());
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

        mTabListCallback = new TabListCallback() {
            @Override
            public void onTabAdded(Tab tab) {
                // The first tab can be added asynchronously with session restore enabled.
                if (mTab == null) {
                    setTab(tab);
                }
            }

            @Override
            public void onTabRemoved(Tab tab) {
                mPreviousTabList.remove(tab);

                if (mTab == tab) {
                    Tab prevTab = null;
                    if (!mPreviousTabList.isEmpty()) {
                        prevTab = mPreviousTabList.remove(mPreviousTabList.size() - 1);
                    }

                    setTab(prevTab);
                }
            }
        };

        mBrowser.registerTabListCallback(mTabListCallback);

        if (mBrowser.getActiveTab() == null) {
            // This happens with session restore enabled.
            assert mBrowser.getTabs().size() == 0;
        } else {
            setTab(mBrowser.getActiveTab());
        }
    }

    // Clears the state associated with |mTab| and sets |tab|, if non-null, as |mTab| and the
    // active tab in the browser.
    private void setTab(Tab tab) {
        if (mTab != null) {
            mTab.unregisterTabCallback(mTabCallback);
            mTabCallback = null;
            mTab = null;
        }

        mTab = tab;

        if (mTab == null) return;

        // TODO(crbug.com/1066382): This will not be correct in the case where the initial
        // navigation in |tab| was a failed navigation and there have been no more navigations since
        // then.
        mUrlView.setText(getLastCommittedUrlInTab(mTab));

        mTabCallback = new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        };
        mTab.registerTabCallback(mTabCallback);

        mTab.setNewTabCallback(new NewTabCallback() {
            @Override
            public void onNewTab(Tab newTab, @NewTabType int type) {
                mPreviousTabList.add(mTab);
                setTab(newTab);
            }
            @Override
            public void onCloseTab() {
                assert false;
            }
        });

        // Will be a no-op if this tab is already the active tab.
        mBrowser.setActiveTab(mTab);
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

    // Returns the display URL of the last committed navigation entry in |tab|. This will
    // return an empty URL if there have been no committed navigations in |tab|.
    public String getLastCommittedUrlInTab(Tab tab) {
        NavigationController navController = tab.getNavigationController();
        int currentIndex = navController.getNavigationListCurrentIndex();
        return currentIndex == -1
                ? ""
                : navController.getNavigationEntryDisplayUri(currentIndex).toString();
    }

    public String getCurrentDisplayUrl() {
        return mUrlView.getText().toString();
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
