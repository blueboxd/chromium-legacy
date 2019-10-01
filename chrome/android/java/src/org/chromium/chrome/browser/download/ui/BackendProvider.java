// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.components.offline_items_collection.ContentId;

/**
 * Provides classes that need to be interacted with by the download UI. This interface will be
 * removed once {@link OfflineContentProvider} is enabled for downloads.
 */
public interface BackendProvider {

    /** Interacts with the Downloads backend. */
    public static interface DownloadDelegate {
        /** See {@link DownloadManagerService#addDownloadObserver}. */
        void addDownloadObserver(DownloadObserver observer);

        /** See {@link DownloadManagerService#removeDownloadObserver}. */
        void removeDownloadObserver(DownloadObserver observer);

        /** See {@link DownloadManagerService#getAllDownloads}. */
        void getAllDownloads(boolean isOffTheRecord);

        /** See {@link DownloadManagerService#broadcastDownloadAction}. */
        void broadcastDownloadAction(DownloadItem downloadItem, String action);

        /** See {@link DownloadManagerService#checkForExternallyRemovedDownloads}. */
        void checkForExternallyRemovedDownloads(boolean isOffTheRecord);

        /** See {@link DownloadManagerService#removeDownload}. */
        void removeDownload(String guid, boolean isOffTheRecord, boolean externallyRemoved);

        /** See {@link DownloadManagerService#isDownloadOpenableInBrowser}. */
        boolean isDownloadOpenableInBrowser(boolean isOffTheRecord, String mimeType);

        /** See {@link DownloadManagerService#updateLastAccessTime}. */
        void updateLastAccessTime(String downloadGuid, boolean isOffTheRecord);

        /** See {@link DownloadManagerService#renameDownload}. */
        void renameDownload(ContentId id, String name, Callback<Integer /*RenameResult*/> callback,
                boolean isOffTheRecord);
    }
}
