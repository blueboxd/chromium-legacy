// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.webkit.ValueCallback;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.ViewSwitcher;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.weblayer.Browser;
import org.chromium.weblayer.ContextMenuParams;
import org.chromium.weblayer.DownloadCallback;
import org.chromium.weblayer.ErrorPageCallback;
import org.chromium.weblayer.FindInPageCallback;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.NewTabType;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.UrlBarOptions;
import org.chromium.weblayer.WebLayer;

import java.util.ArrayList;
import java.util.List;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends FragmentActivity {
    private static class ContextMenuCreator
            implements View.OnCreateContextMenuListener, MenuItem.OnMenuItemClickListener {
        private static final int MENU_ID_COPY_LINK_URI = 1;
        private static final int MENU_ID_COPY_LINK_TEXT = 2;

        private ContextMenuParams mParams;
        private Context mContext;

        public ContextMenuCreator(ContextMenuParams params) {
            mParams = params;
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContext = v.getContext();
            menu.add(mParams.pageUri.toString());
            if (mParams.linkUri != null) {
                MenuItem copyLinkUriItem =
                        menu.add(Menu.NONE, MENU_ID_COPY_LINK_URI, Menu.NONE, "Copy link address");
                copyLinkUriItem.setOnMenuItemClickListener(this);
            }
            if (!TextUtils.isEmpty(mParams.linkText)) {
                MenuItem copyLinkTextItem =
                        menu.add(Menu.NONE, MENU_ID_COPY_LINK_TEXT, Menu.NONE, "Copy link text");
                copyLinkTextItem.setOnMenuItemClickListener(this);
            }
            if (!TextUtils.isEmpty(mParams.titleOrAltText)) {
                TextView altTextView = new TextView(mContext);
                altTextView.setText(mParams.titleOrAltText);
                menu.setHeaderView(altTextView);
            }
            v.setOnCreateContextMenuListener(null);
        }

        @Override
        public boolean onMenuItemClick(MenuItem item) {
            ClipboardManager clipboard =
                    (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            switch (item.getItemId()) {
                case MENU_ID_COPY_LINK_URI:
                    clipboard.setPrimaryClip(
                            ClipData.newPlainText("link address", mParams.linkUri.toString()));
                    break;
                case MENU_ID_COPY_LINK_TEXT:
                    clipboard.setPrimaryClip(ClipData.newPlainText("link text", mParams.linkText));
                    break;
                default:
                    break;
            }
            return true;
        }
    }

    private static final String TAG = "WebLayerShell";
    private static final String KEY_MAIN_VIEW_ID = "mainViewId";
    private static final float DEFAULT_TEXT_SIZE = 15.0F;
    private static final int EDITABLE_URL_TEXT_VIEW = 0;
    private static final int NONEDITABLE_URL_TEXT_VIEW = 1;

    private Profile mProfile;
    private Browser mBrowser;
    private ImageButton mMenuButton;
    private ViewSwitcher mUrlViewContainer;
    private EditText mEditUrlView;
    private ProgressBar mLoadProgressBar;
    private View mMainView;
    private int mMainViewId;
    private View mTopContentsContainer;
    private TabListCallback mTabListCallback;
    private List<Tab> mPreviousTabList = new ArrayList<>();
    private Runnable mExitFullscreenRunnable;
    private View mBottomView;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout mainView = new LinearLayout(this);
        mainView.setOrientation(LinearLayout.VERTICAL);
        TextView versionText = new TextView(this);
        versionText.setPadding(10, 0, 0, 0);
        versionText.setText(getString(
                R.string.version, WebLayer.getVersion(), WebLayer.getSupportedFullVersion(this)));
        mainView.addView(versionText,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        if (savedInstanceState == null) {
            mMainViewId = View.generateViewId();
        } else {
            mMainViewId = savedInstanceState.getInt(KEY_MAIN_VIEW_ID);
        }
        mainView.setId(mMainViewId);
        mMainView = mainView;
        setContentView(mainView);

        mTopContentsContainer =
                LayoutInflater.from(this).inflate(R.layout.shell_browser_controls, null);
        mUrlViewContainer = mTopContentsContainer.findViewById(R.id.url_view_container);

        mEditUrlView = mUrlViewContainer.findViewById(R.id.editable_url_view);
        mEditUrlView.setOnEditorActionListener((TextView v, int actionId, KeyEvent event) -> {
            loadUrl(mEditUrlView.getText().toString());
            mEditUrlView.clearFocus();
            return true;
        });
        mUrlViewContainer.setDisplayedChild(EDITABLE_URL_TEXT_VIEW);

        mMenuButton = mTopContentsContainer.findViewById(R.id.menu_button);
        mMenuButton.setOnClickListener(v -> {
            PopupMenu popup = new PopupMenu(WebLayerShellActivity.this, v);
            popup.getMenuInflater().inflate(R.menu.app_menu, popup.getMenu());
            MenuItem bottomMenuItem = popup.getMenu().findItem(R.id.toggle_bottom_view_id);
            bottomMenuItem.setChecked(mBottomView != null);
            popup.setOnMenuItemClickListener(item -> {
                if (item.getItemId() == R.id.reload_menu_id) {
                    mBrowser.getActiveTab().getNavigationController().reload();
                    return true;
                }

                if (item.getItemId() == R.id.find_begin_menu_id) {
                    // TODO(estade): add a UI for FIP. For now, just search for "cat", or go
                    // to the next result if a search has already been initiated.
                    mBrowser.getActiveTab().getFindInPageController().setFindInPageCallback(
                            new FindInPageCallback() {});
                    mBrowser.getActiveTab().getFindInPageController().find("cat", true);
                    return true;
                }

                if (item.getItemId() == R.id.find_end_menu_id) {
                    mBrowser.getActiveTab().getFindInPageController().setFindInPageCallback(null);
                    return true;
                }

                if (item.getItemId() == R.id.toggle_bottom_view_id) {
                    if (mBottomView == null) {
                        mBottomView =
                                LayoutInflater.from(this).inflate(R.layout.bottom_controls, null);
                    } else {
                        mBottomView = null;
                    }
                    mBrowser.setBottomView(mBottomView);
                    return true;
                }

                return false;
            });
            popup.show();
        });

        mLoadProgressBar = mTopContentsContainer.findViewById(R.id.progress_bar);

        try {
            // This ensures asynchronous initialization of WebLayer on first start of activity.
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened.
            WebLayer.loadAsync(
                    getApplication(), webLayer -> onWebLayerReady(webLayer, savedInstanceState));
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mUrlViewContainer.reset();
        if (mTabListCallback != null) {
            mBrowser.unregisterTabListCallback(mTabListCallback);
            mTabListCallback = null;
        }
    }

    private void onWebLayerReady(WebLayer webLayer, Bundle savedInstanceState) {
        if (isFinishing() || isDestroyed()) return;

        webLayer.setRemoteDebuggingEnabled(true);

        Fragment fragment = getOrCreateBrowserFragment(savedInstanceState);

        // Have WebLayer Shell retain the fragment instance to simulate the behavior of
        // external embedders (note that if this is changed, then WebLayer Shell should handle
        // rotations and resizes itself via its manifest, as otherwise the user loses all state
        // when the shell is rotated in the foreground).
        fragment.setRetainInstance(true);
        mBrowser = Browser.fromFragment(fragment);
        mProfile = mBrowser.getProfile();
        setTabCallbacks(mBrowser.getActiveTab(), fragment);

        mBrowser.setTopView(mTopContentsContainer);
        mTabListCallback = new TabListCallback() {
            @Override
            public void onActiveTabChanged(Tab activeTab) {
                mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);
            }
            @Override
            public void onTabRemoved(Tab tab) {
                closeTab(tab);
            }
        };
        mBrowser.registerTabListCallback(mTabListCallback);
        View nonEditUrlView = mBrowser.getUrlBarController().createUrlBarView(
                UrlBarOptions.builder()
                        .setTextSizeSP(DEFAULT_TEXT_SIZE)
                        .setTextColor(android.R.color.black)
                        .setIconColor(android.R.color.black)
                        .build());
        nonEditUrlView.setOnClickListener(v -> {
            mEditUrlView.setText("");
            mUrlViewContainer.setDisplayedChild(EDITABLE_URL_TEXT_VIEW);
            mEditUrlView.requestFocus();
        });
        mUrlViewContainer.removeViewAt(NONEDITABLE_URL_TEXT_VIEW);
        mUrlViewContainer.addView(nonEditUrlView, NONEDITABLE_URL_TEXT_VIEW);
        mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);

        if (getCurrentDisplayUrl() != null) {
            return;
        }
        String startupUrl = getUrlFromIntent(getIntent());
        if (TextUtils.isEmpty(startupUrl)) {
            startupUrl = "https://google.com";
        }
        loadUrl(startupUrl);
    }

    /* Returns the Url for the current tab as a String, or null if there is no
     * current tab. */
    private String getCurrentDisplayUrl() {
        NavigationController navigationController =
                mBrowser.getActiveTab().getNavigationController();

        if (navigationController.getNavigationListSize() == 0) {
            return null;
        }

        return navigationController
                .getNavigationEntryDisplayUri(navigationController.getNavigationListCurrentIndex())
                .toString();
    }

    private void setTabCallbacks(Tab tab, Fragment fragment) {
        tab.setNewTabCallback(new NewTabCallback() {
            @Override
            public void onNewTab(Tab newTab, @NewTabType int type) {
                setTabCallbacks(newTab, fragment);
                mPreviousTabList.add(mBrowser.getActiveTab());
                mBrowser.setActiveTab(newTab);
            }
            @Override
            public void onCloseTab() {
                // This callback is deprecated and no longer sent.
                assert false;
            }
        });
        tab.setFullscreenCallback(new FullscreenCallback() {
            private int mSystemVisibilityToRestore;

            @Override
            public void onEnterFullscreen(Runnable exitFullscreenRunnable) {
                mExitFullscreenRunnable = exitFullscreenRunnable;
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
                        | View.SYSTEM_UI_FLAG_LOW_PROFILE | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }

            @Override
            public void onExitFullscreen() {
                mExitFullscreenRunnable = null;
                View decorView = getWindow().getDecorView();
                decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

                final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS) != 0) {
                    attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                    getWindow().setAttributes(attrs);
                }
            }
        });
        tab.registerTabCallback(new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlViewContainer.setDisplayedChild(NONEDITABLE_URL_TEXT_VIEW);
            }

            @Override
            public void onTabModalStateChanged(boolean isTabModalShowing) {
                mMenuButton.setEnabled(!isTabModalShowing);
            }

            @Override
            public void showContextMenu(ContextMenuParams params) {
                View webLayerView = getSupportFragmentManager().getFragments().get(0).getView();
                webLayerView.setOnCreateContextMenuListener(new ContextMenuCreator(params));
                webLayerView.showContextMenu();
            }

            @Override
            public void bringTabToFront() {
                tab.getBrowser().setActiveTab(tab);

                Context context = WebLayerShellActivity.this;
                Intent intent = new Intent(context, WebLayerShellActivity.class);
                intent.setAction(Intent.ACTION_MAIN);
                context.getApplicationContext().startActivity(intent);
            }
        });
        tab.getNavigationController().registerNavigationCallback(new NavigationCallback() {
            @Override
            public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {
                mLoadProgressBar.setVisibility(
                        isLoading && toDifferentDocument ? View.VISIBLE : View.INVISIBLE);
            }

            @Override
            public void onLoadProgressChanged(double progress) {
                mLoadProgressBar.setProgress((int) Math.round(100 * progress));
            }
        });
        mProfile.setDownloadCallback(new DownloadCallback() {
            @Override
            public boolean onInterceptDownload(Uri uri, String userAgent, String contentDisposition,
                    String mimetype, long contentLength) {
                return false;
            }

            @Override
            public void allowDownload(Uri uri, String requestMethod, Uri requestInitiator,
                    ValueCallback<Boolean> callback) {
                callback.onReceiveValue(true);
            }
        });
        tab.setErrorPageCallback(new ErrorPageCallback() {
            @Override
            public boolean onBackToSafety() {
                fragment.getActivity().onBackPressed();
                return true;
            }
        });
    }

    private void closeTab(Tab tab) {
        mPreviousTabList.remove(tab);
        if (mBrowser.getActiveTab() == null && !mPreviousTabList.isEmpty()) {
            mBrowser.setActiveTab(mPreviousTabList.remove(mPreviousTabList.size() - 1));
        }
    }

    private Fragment getOrCreateBrowserFragment(Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return fragments.get(0);
            }
        }

        String profileName = "DefaultProfile";
        Fragment fragment = WebLayer.createBrowserFragment(profileName);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(mMainViewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
    }

    public void loadUrl(String url) {
        mBrowser.getActiveTab().getNavigationController().navigate(Uri.parse(sanitizeUrl(url)));
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
        if (mBrowser != null) {
            Tab activeTab = mBrowser.getActiveTab();

            if (activeTab.dismissTransientUi()) return;

            NavigationController controller = activeTab.getNavigationController();
            if (controller.canGoBack()) {
                controller.goBack();
                return;
            }
            if (!mPreviousTabList.isEmpty()) {
                activeTab.dispatchBeforeUnloadAndClose();
                return;
            }
        }
        super.onBackPressed();
    }
}
