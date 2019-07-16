// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Bundle;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.glue.OfflineContentProviderGlue;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.touchless.R;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.widget.ChromeImageView;

/**
 * An activity that reacts to download notifications on touchless devices. This activity is 1:1
 * with each download and will only show the title of the download and actions to pause, resume, and
 * cancel.
 */
public class TouchlessDownloadActivity extends SynchronousInitializationActivity {
    /** The title of the download UI. */
    private TextView mDownloadTitle;

    /** The view containing the text for the pause/resume button. */
    private TextView mPauseResumeText;

    /** The view for the icon shown for the pause/resume option. */
    private ChromeImageView mPauseResumeIcon;

    /** A handle to the offline item representing the download. */
    private OfflineItem mOfflineItem;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Hard-code setIsOffTheRecord to false since incognito is unsupported in touchless mode.
        DownloadManagerUiConfig config = new DownloadManagerUiConfig.Builder()
                                                 .setIsOffTheRecord(false)
                                                 .setIsSeparateActivity(true)
                                                 .build();

        OfflineContentProviderGlue downloadGlue =
                new OfflineContentProviderGlue(OfflineContentAggregatorFactory.get(), config);

        final ContentId id = DownloadBroadcastManager.getContentIdFromIntent(getIntent());

        // If there's no ID for the download, finish the activity.
        if (id == null) finish();

        ViewGroup group =
                (ViewGroup) getLayoutInflater().inflate(R.layout.touchless_download_options, null);
        setContentView(group);
        mDownloadTitle = (TextView) group.findViewById(R.id.title);
        mPauseResumeIcon = (ChromeImageView) group.findViewById(R.id.pause_resume_icon);
        mPauseResumeText = (TextView) group.findViewById(R.id.pause_resume_text);
        updateVisualsForOfflineItem(null);

        // TODO(976419): Use getItemById once implemented.
        downloadGlue.getAllItems((items) -> {
            for (OfflineItem item : items) {
                if (item.id.equals(id)) {
                    mOfflineItem = item;
                    break;
                }
            }

            // Don't show the UI if there's no download to display.
            if (mOfflineItem == null) finish();

            updateVisualsForOfflineItem(mOfflineItem);
        });

        // Handle clicks on the cancel option.
        View cancelOption = group.findViewById(R.id.cancel_option);
        cancelOption.setOnClickListener((v) -> {
            if (mOfflineItem == null) return;
            downloadGlue.cancelDownload(mOfflineItem);
            finish();
        });

        View pauseOption = group.findViewById(R.id.pause_resume_option);
        pauseOption.setOnClickListener((v) -> {
            if (mOfflineItem == null) return;
            if (mOfflineItem.state != OfflineItemState.PAUSED) {
                downloadGlue.pauseDownload(mOfflineItem);
            } else {
                downloadGlue.resumeDownload(mOfflineItem, true);
            }
            finish();
        });
    }

    /**
     * Update the state of the UI to reflect the offline item.
     * @param item The offline item.
     */
    private void updateVisualsForOfflineItem(OfflineItem item) {
        mDownloadTitle.setText(item != null ? item.title : "");

        boolean isPaused = item != null && item.state == OfflineItemState.PAUSED;

        @DrawableRes
        int iconId = isPaused ? R.drawable.ic_play_circle_outline_24dp
                              : R.drawable.ic_pause_circle_outline_24dp;
        @StringRes
        int textId = isPaused ? org.chromium.chrome.R.string.download_notification_resume_button
                              : org.chromium.chrome.R.string.download_notification_pause_button;

        mPauseResumeIcon.setImageDrawable(
                ApiCompatibilityUtils.getDrawable(getResources(), iconId));
        mPauseResumeText.setText(textId);
    }
}
