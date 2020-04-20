// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * Class for saving most visited sites info.
 */
public class MostVisitedSitesHost implements MostVisitedSites.Observer {
    private static final String TAG = "TopSites";

    // The map mapping URL to faviconId. This map will be updated once there are new suggestions
    // available.
    private final Map<String, Integer> mUrlToIdMap = new HashMap<>();

    // The map mapping faviconId to URL. This map will be reconstructed based on the mUrlToIdMap
    // once there are new suggestions available.
    private final Map<Integer, String> mIdToUrlMap = new HashMap<>();

    // The set of URLs needed to fetch favicon. This set will be reconstructed once there are new
    // suggestions available.
    private final Set<String> mUrlsToUpdateFavicon = new HashSet<>();

    private final MostVisitedSites mMostVisitedSites;
    private final MostVisitedSitesFaviconHelper mFaviconSaver;

    private static boolean sSkipRestoreFromDiskForTests;

    public MostVisitedSitesHost(Context context, Profile profile) {
        LargeIconBridge largeIconBridge =
                SuggestionsDependencyFactory.getInstance().createLargeIconBridge(profile);
        mFaviconSaver = new MostVisitedSitesFaviconHelper(context, largeIconBridge);
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(profile);
        if (!sSkipRestoreFromDiskForTests) {
            restoreFromDisk();
        }
    }

    /**
     * Restore disk info to the mUrlToIDMap.
     */
    private void restoreFromDisk() {
        new AsyncTask<List<SiteSuggestion>>() {
            @Override
            protected List<SiteSuggestion> doInBackground() {
                List<SiteSuggestion> siteSuggestions = new ArrayList<>();
                try {
                    siteSuggestions = MostVisitedSitesMetadataUtils.restoreFileToSuggestionLists();
                } catch (IOException e) {
                    Log.i(TAG, "No top sites lists file existed in the disk.");
                }
                return siteSuggestions;
            }

            @Override
            protected void onPostExecute(List<SiteSuggestion> siteSuggestions) {
                mUrlToIdMap.clear();
                mIdToUrlMap.clear();
                for (SiteSuggestion site : siteSuggestions) {
                    mUrlToIdMap.put(site.url, site.faviconId);
                }
                buildIdToUrlMap();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    // TODO(spdonghao): Add a queue to cache the new SiteSuggestions task when there is a pending
    // task that is updating the favicon files and metadata on disk. Reschedule the cached task when
    // the pending task is completed.
    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        updateMapAndSetForNewSites(siteSuggestions, () -> {
            MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(siteSuggestions, null);
            mFaviconSaver.saveFaviconsToFile(siteSuggestions, mUrlsToUpdateFavicon);
        });
    }

    @Override
    public void onIconMadeAvailable(String siteUrl) {}

    /**
     * Start the observer.
     * @param maxResults The max number of sites to observe.
     */
    public void startObserving(int maxResults) {
        mMostVisitedSites.setObserver(this, maxResults);
    }

    /**
     * Update mUrlToIDMap and mUrlsToUpdateFavicon based on the new SiteSuggestions passed in.
     * @param newSuggestions The new SiteSuggestions.
     * @param callback The callback function after updating map and set.
     */
    @VisibleForTesting
    protected void updateMapAndSetForNewSites(
            List<SiteSuggestion> newSuggestions, Runnable callback) {
        new AsyncTask<Set<String>>() {
            @Override
            protected Set<String> doInBackground() {
                return getExistingIconFiles();
            }

            @Override
            protected void onPostExecute(Set<String> existingIconFiles) {
                // Clear mUrlsToUpdateFavicon.
                mUrlsToUpdateFavicon.clear();
                // Update mIdToUrlMap with current mUrlToIdMap.
                buildIdToUrlMap();

                // Update mUrlsToUpdateFavicon based on the new SiteSuggestions.
                refreshUrlsToUpdate(newSuggestions, existingIconFiles);

                // Update mUrlToIdMap based on the new SiteSuggestions.
                updateMapForNewSites(newSuggestions, callback);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private static Set<String> getExistingIconFiles() {
        Set<String> existingIconFiles = new HashSet<>();
        File topSitesDirectory = MostVisitedSitesMetadataUtils.getStateDirectory();

        if (topSitesDirectory == null || topSitesDirectory.list() == null) {
            return existingIconFiles;
        }

        existingIconFiles.addAll(Arrays.asList(Objects.requireNonNull(topSitesDirectory.list())));
        return existingIconFiles;
    }

    /**
     * Update mUrlsToUpdateFavicon based on the new SiteSuggestions passed in.
     * @param newSuggestions The new SiteSuggestions.
     * @param existingIconFiles The existing favicon files name set.
     */
    private void refreshUrlsToUpdate(
            List<SiteSuggestion> newSuggestions, Set<String> existingIconFiles) {
        // Add topsites URLs which need to fetch icon to the mUrlsToUpdateFavicon Set.
        for (SiteSuggestion topSiteData : newSuggestions) {
            String url = topSiteData.url;
            // If the old map doesn't contain the URL or there is no favicon file for this URL, then
            // add this URL to mUrlsToUpdateFavicon.
            if (!mUrlToIdMap.containsKey(url)
                    || !existingIconFiles.contains(String.valueOf(mUrlToIdMap.get(url)))) {
                mUrlsToUpdateFavicon.add(url);
            }
        }
    }

    /**
     * Update the mUrlToIdMap based on the new SiteSuggestions and mUrlsToUpdateFavicon.
     * @param newSuggestions The new SiteSuggestions.
     * @param callback The callback function after updating map and set.
     */
    private void updateMapForNewSites(List<SiteSuggestion> newSuggestions, Runnable callback) {
        // Get the set of new top sites' URLs.
        Set<String> newUrls = new HashSet<>();
        for (SiteSuggestion topSiteData : newSuggestions) {
            newUrls.add(topSiteData.url);
        }

        // Add new URLs and ids to the mUrlToIDMap.
        int id = 0;
        for (String url : mUrlsToUpdateFavicon) {
            if (mUrlToIdMap.containsKey(url)) {
                continue;
            }
            // Get the next available ID.
            id = getNextAvailableId(id, newUrls);
            mUrlToIdMap.put(url, id);
            id++;
        }

        // Remove stale data in mUrlToIdMap.
        List<Integer> idsToDeleteFile = removeStaleData(newUrls);

        // Remove stale favicon files in the disk asynchronously.
        deleteStaleFilesAsync(idsToDeleteFile, () -> {
            mIdToUrlMap.clear();

            // Save faviconIDs to newSuggestions.
            for (SiteSuggestion siteData : newSuggestions) {
                siteData.faviconId = Objects.requireNonNull(mUrlToIdMap.get(siteData.url));
            }

            if (callback != null) {
                callback.run();
            }
        });
    }

    @VisibleForTesting
    protected int getNextAvailableId(int start, Set<String> newTopSiteUrls) {
        int id = start;
        // The available ids should be in range [0, newTopSiteUrls.size()), since we only need
        // |newTopSiteUrls.size()| ids.
        for (; id < newTopSiteUrls.size(); id++) {
            // If this id is not used in mUrlToIdMap or URL corresponding to this id is not in
            // the newTopSiteURLs, it's available.
            if (!mIdToUrlMap.containsKey(id) || !newTopSiteUrls.contains(mIdToUrlMap.get(id))) {
                return id;
            }
        }
        assert false : "Unreachable code.";
        return id;
    }

    @VisibleForTesting
    protected void buildIdToUrlMap() {
        mIdToUrlMap.clear();
        for (Map.Entry<String, Integer> entry : mUrlToIdMap.entrySet()) {
            mIdToUrlMap.put(entry.getValue(), entry.getKey());
        }
    }

    /**
     * Return faviconIds which need to delete files.
     * @param newUrls The URLs in new SiteSuggestions.
     * @return The list of faviconIds needed to remove.
     */
    private List<Integer> removeStaleData(Set<String> newUrls) {
        List<Integer> idsToDeleteFile = new ArrayList<>();
        for (Iterator<Map.Entry<String, Integer>> it = mUrlToIdMap.entrySet().iterator();
                it.hasNext();) {
            Map.Entry<String, Integer> entry = it.next();
            String url = entry.getKey();
            int faviconId = entry.getValue();
            if (!newUrls.contains(url)) {
                it.remove();
                idsToDeleteFile.add(faviconId);
            }
        }
        return idsToDeleteFile;
    }

    private void deleteStaleFilesAsync(List<Integer> idsToDeleteFile, Runnable callback) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                for (int id : idsToDeleteFile) {
                    File file =
                            new File(MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory(),
                                    String.valueOf(id));
                    file.delete();
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {
                if (callback != null) {
                    callback.run();
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    protected Map<String, Integer> getUrlToIDMapForTesting() {
        return mUrlToIdMap;
    }

    @VisibleForTesting
    protected Set<String> getUrlsToUpdateFaviconForTesting() {
        return mUrlsToUpdateFavicon;
    }

    @VisibleForTesting
    protected Map<Integer, String> getIdToUrlMapForTesting() {
        return mIdToUrlMap;
    }

    @VisibleForTesting
    protected static void setSkipRestoreFromDiskForTesting() {
        sSkipRestoreFromDiskForTests = true;
    }
}
