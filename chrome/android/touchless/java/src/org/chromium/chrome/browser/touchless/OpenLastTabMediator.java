// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static android.text.format.DateUtils.MINUTE_IN_MILLIS;
import static android.text.format.DateUtils.getRelativeTimeSpanString;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator used to look for history events and update the model accordingly.
 */
// TODO(crbug.com/948858): Add unit tests for this behavior.
class OpenLastTabMediator extends EmptyTabObserver
        implements HistoryProvider.BrowsingHistoryObserver, FocusableComponent {
    private static final String LAST_TAB_URL = "TOUCHLESS_LAST_TAB_URL";

    private final Context mContext;
    private final Profile mProfile;
    private final NativePageHost mNativePageHost;

    private final PropertyModel mModel;

    private BrowsingHistoryBridge mHistoryBridge;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mIconBridge;

    private HistoryItem mHistoryItem;
    private List<HistoryItem> mHistoryResult;
    private boolean mPreferencesRead;
    private String mLastTabUrl;

    /**
     * Sets up an observer on the tab to track the last tab that was opened. The purpose of this is
     * to filter out history results that come from other activities, like PWAs.
     * @param activity the activity to lookup shared prefs with.
     * @param activityTabProvider the source of the tab navigation events.
     * @return a new observer that can be destroyed on shutdown.
     */
    public static ActivityTabProvider.ActivityTabTabObserver createActivityScopedObserver(
            Activity activity, ActivityTabProvider activityTabProvider) {
        return new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                String url = tab.getUrl();
                if (navigation.isInMainFrame() && !NativePageFactory.isNativePageUrl(url, false)) {
                    PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
                        SharedPreferences prefs = getSharedPreferences(activity);
                        prefs.edit()
                                .putString(OpenLastTabMediator.LAST_TAB_URL, tab.getUrl())
                                .apply();
                    });
                }
            }
        };
    }

    OpenLastTabMediator(Context context, Profile profile, NativePageHost nativePageHost,
            PropertyModel model, OpenLastTabView view) {
        mModel = model;
        mContext = context;
        mProfile = profile;
        mNativePageHost = nativePageHost;

        mHistoryBridge = new BrowsingHistoryBridge(false);
        mHistoryBridge.setObserver(this);
        mNativePageHost.getActiveTab().addObserver(this);

        mIconGenerator =
                ViewUtils.createDefaultRoundedIconGenerator(mContext.getResources(), false);
        mIconBridge = new LargeIconBridge(mProfile);

        readPreferences();

        // TODO(wylieb):Investigate adding an item limit to the API.
        // Query the history for everything (no API exists to only query for the most recent).
        refreshHistoryData();
    }

    private SharedPreferences getSharedPreferences() {
        return getSharedPreferences(mNativePageHost.getActiveTab().getActivity());
    }

    private static SharedPreferences getSharedPreferences(Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE);
    }

    private void readPreferences() {
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            // Check if this is a first launch of Chrome.
            SharedPreferences prefs = getSharedPreferences();
            mLastTabUrl = prefs.getString(LAST_TAB_URL, null);
            PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, () -> {
                mPreferencesRead = true;
                updateModel();
            });
        });
    }

    void destroy() {
        if (mHistoryBridge != null) {
            mHistoryBridge.destroy();
            mHistoryBridge = null;
        }
        mNativePageHost.getActiveTab().removeObserver(this);
    }

    void refreshHistoryData() {
        mHistoryBridge.queryHistory("");
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        // Query the history as it may have been cleared while the app was hidden. This could happen
        // via the Android settings.
        refreshHistoryData();
    }

    @Override
    public void requestFocus() {
        mModel.set(OpenLastTabProperties.SHOULD_FOCUS_VIEW, true);
    }

    @Override
    public void setOnFocusListener(Runnable listener) {
        mModel.set(OpenLastTabProperties.ON_FOCUS_CALLBACK, listener);
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        mHistoryItem = null;
        mHistoryResult = items;
        // Ignore |hasMorePotentialMatches|, it's possible a user with a lot of PWA  history does
        // not get a match in the first query. In this case, we'll simply not show the last tab
        // button. It probably isn't really recent anyways.

        if (mPreferencesRead) {
            updateModel();
        }
    }

    // updateModel is only called after preferences are read. It populates model with data from
    // mHistoryItem.
    private void updateModel() {
        if (mHistoryResult != null) {
            for (HistoryItem item : mHistoryResult) {
                if (item.getUrl().equals(mLastTabUrl)) {
                    mHistoryItem = item;
                    break;
                }
            }
            mHistoryResult = null;
        }

        if (mHistoryItem == null) {
            // Consider the case where the history has nothing in it to be a failure.
            mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS, false);
            return;
        }

        String title = UrlUtilities.getDomainAndRegistry(mHistoryItem.getUrl(), false);
        // Default the timestamp to happening just now. If it happened over a minute ago, calculate
        // and set the relative timestamp string.
        String timestamp = mContext.getResources().getString(R.string.open_last_tab_just_now);
        long now = System.currentTimeMillis();
        if (now - mHistoryItem.getTimestamp() > MINUTE_IN_MILLIS) {
            timestamp =
                    getRelativeTimeSpanString(mHistoryItem.getTimestamp(), now, MINUTE_IN_MILLIS)
                            .toString();
        }

        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_TITLE, title);
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP, timestamp);
        boolean willReturnIcon = mIconBridge.getLargeIconForUrl(mHistoryItem.getUrl(),
                mContext.getResources().getDimensionPixelSize(R.dimen.open_last_tab_icon_size),
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    setAndShowButton(mHistoryItem.getUrl(), icon, fallbackColor, title);
                });

        // False if icon bridge won't call us back.
        if (!willReturnIcon) {
            setAndShowButton(mHistoryItem.getUrl(), null, R.color.default_icon_color, title);
        }
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS, true);
    }

    @Override
    public void onHistoryDeleted() {
        refreshHistoryData();
    }

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {}

    private void setAndShowButton(String url, Bitmap icon, int fallbackColor, String title) {
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER, view -> {
            mNativePageHost.loadUrl(new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK),
                    /* Explore page is never off the record. */ false);
        });

        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(url);
        }
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_FAVICON, icon);

        final Bitmap shortcutIcon = icon;
        TouchlessContextMenuManager.Delegate delegate =
                new TouchlessContextMenuManager.EmptyDelegate() {
                    @Override
                    public boolean isItemSupported(
                            @ContextMenuManager.ContextMenuItemId int menuItemId) {
                        return menuItemId == ContextMenuItemId.SEARCH
                                || menuItemId == ContextMenuManager.ContextMenuItemId.REMOVE;
                    }

                    @Override
                    public void removeItem() {
                        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
                            SharedPreferences prefs = getSharedPreferences();
                            prefs.edit().remove(LAST_TAB_URL).apply();
                        });
                        // Don't actually remove the element from history db. This is fine since
                        // we're tracking it in the pref, the open last tab button will not show it.
                        // If this history item is backed by 2+ rows a history deletion could
                        // trigger a clear and suppression of suggested articles, see
                        // https://crbug.com/982973.
                        mHistoryItem = null;
                        mLastTabUrl = null;
                        updateModel();
                    }

                    @Override
                    public String getUrl() {
                        return url;
                    }

                    @Override
                    public String getContextMenuTitle() {
                        return title;
                    }

                    @Override
                    public String getTitle() {
                        return title;
                    }

                    @Override
                    public Bitmap getIconBitmap() {
                        return shortcutIcon;
                    }
                };
        mModel.set(OpenLastTabProperties.CONTEXT_MENU_DELEGATE, delegate);
    }
}
