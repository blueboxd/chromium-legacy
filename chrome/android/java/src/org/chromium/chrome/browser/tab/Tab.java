// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tab is a visual/functional unit that encapsulates the content (not just web site content
 * from network but also other types of content such as NTP, navigation history, etc) and
 * presents it to users who perceive it as one of the 'pages' managed by Chrome.
 */
public interface Tab {
    public static final int INVALID_TAB_ID = -1;

    /**
     * A list of the various ways tabs can be hidden.
     */
    @IntDef({TabHidingType.CHANGED_TABS, TabHidingType.ACTIVITY_HIDDEN, TabHidingType.REPARENTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabHidingType {
        /** A tab was hidden due to other tab getting foreground. */
        int CHANGED_TABS = 0;

        /** A tab was hidden together with an activity. */
        int ACTIVITY_HIDDEN = 1;

        /** A tab was hidden while being reparented to a new activity. */
        int REPARENTED = 2;
    }

    /**
     * Adds a {@link TabObserver} to be notified on {@link Tab} changes.
     * @param observer The {@link TabObserver} to add.
     */
    void addObserver(TabObserver observer);

    /**
     * Removes a {@link TabObserver}.
     * @param observer The {@link TabObserver} to remove.
     */
    void removeObserver(TabObserver observer);

    /**
     * @return {@link UserDataHost} that manages {@link UserData} objects attached to.
     *         This is used for managing Tab-specific attributes/objects without Tab
     *         object having to know about them directly.
     */
    UserDataHost getUserDataHost();

    /**
     * @return The web contents associated with this tab.
     */
    @Nullable
    WebContents getWebContents();

    /**
     * @return The {@link Activity} {@link Context} if this {@link Tab} is attached to an
     *         {@link Activity}, otherwise the themed application context (e.g. hidden tab or
     *         browser action tab).
     */
    @NonNull
    Context getContext();

    /**
     * @return The {@link WindowAndroid} associated with this {@link Tab}.
     */
    WindowAndroid getWindowAndroid();

    /**
     * Update the attachment state to Window(Activity).
     * @param window A new {@link WindowAndroid} to attach the tab to. If {@code null},
     *        the tab is being detached. See {@link ReparentingTask#detach()} for details.
     * @param tabDelegateFactory The new delegate factory this tab should be using. Can be
     *        {@code null} even when {@code window} is not, meaning we simply want to swap out
     *        {@link WindowAndroid} for this tab and keep using the current delegate factory.
     */
    void updateAttachment(
            @Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory);

    /**
     * @return Content view used for rendered web contents. Can be null
     *    if web contents is null.
     */
    ViewGroup getContentView();

    /**
     * @return The {@link View} displaying the current page in the tab. This can be {@code null}, if
     *         the tab is frozen or being initialized or destroyed.
     */
    View getView();

    /**
     * @return The id representing this tab.
     */
    int getId();

    /**
     * @return The id of the tab that caused this tab to be opened.
     */
    int getParentId();

    /**
     * @return The URL that is loaded in the current tab. This may not be the same as
     *         the last committed URL if a new navigation is in progress.
     */
    String getUrl();

    /**
     * @return The tab title.
     */
    String getTitle();

    /**
     * @return The {@link NativePage} associated with the current page, or {@code null} if there is
     *         no current page or the current page is displayed using something besides
     *         {@link NativePage}.
     */
    NativePage getNativePage();

    /**
     * @return Whether or not the {@link Tab} represents a {@link NativePage}.
     */
    boolean isNativePage();

    /**
     * Replaces the current NativePage with a empty stand-in for a NativePage. This can be used
     * to reduce memory pressure.
     */
    void freezeNativePage();

    /**
     * @return The reason the Tab was launched (from a link, external app, etc).
     *         May change over time, for instance, to {@code FROM_RESTORE} during
     *         tab restoration.
     */
    @TabLaunchType
    int getLaunchType();

    /**
     * @return The reason the Tab was launched. This remains unchanged, while {@link
     *         #getLaunchType()} can change over time.
     */
    @Nullable
    @TabLaunchType
    Integer getLaunchTypeAtInitialTabCreation();

    /**
     * @return the last time this tab was shown or the time of its initialization if it wasn't yet
     *         shown.
     */
    long getTimestampMillis();

    /**
     * @return {@code true} if the Tab is in incognito mode.
     */
    boolean isIncognito();

    /**
     * @return Whether the {@link Tab} is currently showing an error page.
     */
    boolean isShowingErrorPage();

    /**
     * @return true iff the tab doesn't hold a live page. For example, this could happen
     *         when the tab holds frozen WebContents state that is yet to be inflated.
     */
    boolean isFrozen();

    /**
     * @return Whether the tab can currently be interacted with by the user.  This requires the
     *         view owned by the Tab to be visible and in a state where the user can interact
     *         with it (i.e. not in something like the phone tab switcher).
     */
    boolean isUserInteractable();

    /**
     * Causes this tab to navigate to the specified URL.
     * @param params parameters describing the url load. Note that it is important to set correct
     *         page transition as it is used for ranking URLs in the history so the omnibox
     *         can report suggestions correctly.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    int loadUrl(LoadUrlParams params);

    /**
     * Loads the tab if it's not loaded (e.g. because it was killed in background).
     * This will trigger a regular load for tabs with pending lazy first load (tabs opened in
     * background on low-memory devices).
     * @return true iff the Tab handled the request.
     */
    boolean loadIfNeeded();

    /**
     * Reloads the current page content.
     */
    void reload();

    /**
     * Reloads the current page content.
     * This version ignores the cache and reloads from the network.
     */
    void reloadIgnoringCache();

    /**
     * Stop the current navigation.
     */
    void stopLoading();

    /**
     * @return Whether the Tab has requested a reload.
     */
    boolean needsReload();

    /**
     * @return true iff the tab is loading and an interstitial page is not showing.
     */
    boolean isLoading();

    /**
     * @return true iff the tab is performing a restore page load.
     */
    boolean isBeingRestored();

    /**
     * @return a value between 0 and 100 reflecting what percentage of the page load is complete.
     */
    float getProgress();

    /**
     * @return Whether or not this tab has a previous navigation entry.
     */
    boolean canGoBack();

    /**
     * @return Whether or not this tab has a navigation entry after the current one.
     */
    boolean canGoForward();

    /**
     * Goes to the navigation entry before the current one.
     */
    void goBack();

    /**
     * Goes to the navigation entry after the current one.
     */
    void goForward();

    /**
     * Sets whether the tab is showing an error page.  This is reset whenever the tab finishes a
     * navigation.
     * Note: This is kept here to keep the build green. Remove from interface as soon as
     *       the downstream patch lands.
     * @param isShowingErrorPage Whether the tab shows an error page.
     */
    void setIsShowingErrorPage(boolean isShowingErrorPage);
}
