// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.explore_sites.ExploreSitesBridge;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCatalogUpdateRequestSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/**
 * Handles SiteSuggestion loading from MostVisitedSites. This includes images and reloading more
 * suggestions when needed.
 */

class SiteSuggestionsMediator implements MostVisitedSites.Observer,
                                         PropertyObservable.PropertyObserver<PropertyKey>,
                                         FocusableComponent {
    private static final int NUM_FETCHED_SITES = 8;
    // The ML tiles could show anywhere between 2-5 tiles. To ensure that the initial index
    // would land on % n == 0 and be roughly Integer.MAX / 2, where n could be [2,5],
    // we divide by 240 (which is 5!) before multiplying by 120.
    // TODO(chili): Maybe better formula for if this needs to be more generic.
    private static final int INITIAL_SCROLLED_POSITION = Integer.MAX_VALUE / 240 * 120;
    private static final int MAX_DISPLAYED_TILES = 4;

    private PropertyModel mModel;
    private ImageFetcher mImageFetcher;
    private MostVisitedSites mMostVisitedSites;
    private Profile mProfile;
    private RoundedIconGenerator mDefaultIconGenerator;
    private int mIconSize;

    // Whether we should set focus to the data. Used to delay focus-setting until after loading
    // pending suggestions.
    private boolean mShouldSetFocus;
    // Whether we have pending requests for suggestions.
    private boolean mHasPendingRequests;

    SiteSuggestionsMediator(PropertyModel model, Profile profile, ImageFetcher imageFetcher,
            RoundedIconGenerator iconGenerator, int minIconSize) {
        mModel = model;
        mImageFetcher = imageFetcher;
        mIconSize = minIconSize;
        mProfile = profile;
        mDefaultIconGenerator = iconGenerator;

        mHasPendingRequests = true;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(mProfile);
        mMostVisitedSites.setObserver(this, NUM_FETCHED_SITES);

        mModel.addObserver(this);

        ExploreSitesBridge.initializeCatalog(
                mProfile, ExploreSitesCatalogUpdateRequestSource.NEW_TAB_PAGE);
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        mHasPendingRequests = false;
        HashSet<String> urls = new HashSet<>();
        for (PropertyModel model : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)) {
            urls.add(model.get(SiteSuggestionModel.URL_KEY));
        }

        // Because of the way we calculate mods, it's better to add suggestions all at once.
        List<PropertyModel> modelList = new ArrayList<>(siteSuggestions.size());

        // Map each SiteSuggestion into a PropertyModel representation and fetch the icon.
        for (SiteSuggestion suggestion : siteSuggestions) {
            // Do not put duplicates.
            if (urls.contains(suggestion.url)) continue;

            PropertyModel siteSuggestion = SiteSuggestionModel.getSiteSuggestionModel(
                    suggestion, mDefaultIconGenerator.generateIconForText(suggestion.title));
            modelList.add(siteSuggestion);
            if (suggestion.whitelistIconPath.isEmpty()) {
                makeIconRequest(siteSuggestion);
            } else {
                AsyncTask<Bitmap> task = new AsyncTask<Bitmap>() {
                    @Override
                    protected Bitmap doInBackground() {
                        return BitmapFactory.decodeFile(suggestion.whitelistIconPath);
                    }
                    @Override
                    protected void onPostExecute(Bitmap icon) {
                        if (icon == null) makeIconRequest(siteSuggestion);
                    }
                };
                task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }
        }

        // If everything was duplicate. don't do anything.
        if (modelList.isEmpty()) return;

        // Total item count is 1 more than number of site suggestions to account for "all apps".
        mModel.set(SiteSuggestionsCoordinator.ITEM_COUNT_KEY,
                getItemCount(modelList.size() + urls.size()));
        mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).addAll(modelList);

        // If we fetched site suggestions the first time, set initial and current position.
        // We don't want to set position if we've already set position before.
        if (siteSuggestions.size() > 0
                && mModel.get(SiteSuggestionsCoordinator.INITIAL_INDEX_KEY) == 0) {
            mModel.set(SiteSuggestionsCoordinator.INITIAL_INDEX_KEY, INITIAL_SCROLLED_POSITION);
            mModel.set(SiteSuggestionsCoordinator.CURRENT_INDEX_KEY, INITIAL_SCROLLED_POSITION);
        }

        if (mShouldSetFocus) {
            mModel.set(SiteSuggestionsCoordinator.SHOULD_FOCUS_VIEW, true);
        }
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {
        // Update icon if an icon was made available.
        for (PropertyModel suggestion : mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY)) {
            if (suggestion.get(SiteSuggestionModel.URL_KEY).equals(siteUrl)) {
                makeIconRequest(suggestion);
            }
        }
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey == SiteSuggestionsCoordinator.REMOVAL_KEY) {
            PropertyModel suggestion = mModel.get(SiteSuggestionsCoordinator.REMOVAL_KEY);
            mMostVisitedSites.addBlacklistedUrl(suggestion.get(SiteSuggestionModel.URL_KEY));

            Toast.makeText(ContextUtils.getApplicationContext(), R.string.most_visited_item_removed,
                         Toast.LENGTH_SHORT)
                    .show();

            // When we remove an item, reset the item count key first.
            int itemCount =
                    getItemCount(mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).size() - 1);
            mModel.set(SiteSuggestionsCoordinator.ITEM_COUNT_KEY, itemCount);
            // Actually remove the suggestion.
            mModel.get(SiteSuggestionsCoordinator.SUGGESTIONS_KEY).remove(suggestion);
            if (itemCount == 1) {
                // If we removed everything except "All Apps", reset the index to 0.
                mModel.set(SiteSuggestionsCoordinator.INITIAL_INDEX_KEY, 0);
                mModel.set(SiteSuggestionsCoordinator.CURRENT_INDEX_KEY, 0);
            }
            // When removal of a site causes us to have fewer sites than we want to display, fetch
            // again.
            if (itemCount < MAX_DISPLAYED_TILES) {
                mHasPendingRequests = true;
                mMostVisitedSites.setObserver(this, NUM_FETCHED_SITES);
            }
        }
    }

    public void destroy() {
        mMostVisitedSites.destroy();
    }

    @Override
    public void requestFocus() {
        if (mHasPendingRequests) {
            mShouldSetFocus = true;
        } else {
            mModel.set(SiteSuggestionsCoordinator.SHOULD_FOCUS_VIEW, true);
        }
    }

    @Override
    public void setOnFocusListener(Runnable listener) {
        mModel.set(SiteSuggestionsCoordinator.ON_FOCUS_CALLBACK, listener);
    }

    private int getItemCount(int listSize) {
        // Item count is number of site suggestions available, up to MAX_DISPLAYED_TILES, + 1 for
        // "All apps".
        return (listSize > MAX_DISPLAYED_TILES ? MAX_DISPLAYED_TILES : listSize) + 1;
    }

    private void makeIconRequest(PropertyModel suggestion) {
        // Fetches the icon for a site suggestion.
        mImageFetcher.makeLargeIconRequest(suggestion.get(SiteSuggestionModel.URL_KEY), mIconSize,
                (@Nullable Bitmap icon, int fallbackColor, boolean isFallbackDefault,
                        int iconType) -> {
                    if (icon != null) {
                        suggestion.set(SiteSuggestionModel.ICON_KEY, icon);
                    }
                });
    }
}
