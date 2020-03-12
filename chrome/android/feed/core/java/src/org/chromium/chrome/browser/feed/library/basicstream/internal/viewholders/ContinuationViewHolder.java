// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.LayoutParams;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.VisibilityMonitor;
import org.chromium.chrome.feed.R;

/** {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} for the more button. */
public class ContinuationViewHolder extends FeedViewHolder {
    private final View mActionButton;
    private final View mSpinner;
    private final VisibilityMonitor mVisibilityMonitor;
    private final CardConfiguration mCardConfiguration;
    private final Context mContext;

    public ContinuationViewHolder(Configuration configuration, Context context,
            FrameLayout frameLayout, CardConfiguration cardConfiguration) {
        super(frameLayout);
        View containerView =
                LayoutInflater.from(context).inflate(R.layout.feed_more_button, frameLayout);
        mActionButton = checkNotNull(frameLayout.findViewById(R.id.action_button));
        mSpinner = checkNotNull(frameLayout.findViewById(R.id.loading_spinner));
        mVisibilityMonitor = createVisibilityMonitor(containerView, configuration);
        this.mCardConfiguration = cardConfiguration;
        this.mContext = context;
    }

    public void bind(
            OnClickListener onClickListener, LoggingListener loggingListener, boolean showSpinner) {
        ViewGroup.LayoutParams layoutParams = itemView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
            itemView.setLayoutParams(layoutParams);
        } else if (!(layoutParams instanceof MarginLayoutParams)) {
            layoutParams = new LayoutParams(layoutParams);
            itemView.setLayoutParams(layoutParams);
        }
        LayoutUtils.setMarginsRelative((MarginLayoutParams) layoutParams,
                mCardConfiguration.getCardStartMargin(),
                (int) mContext.getResources().getDimension(
                        R.dimen.feed_more_button_container_top_margins),
                mCardConfiguration.getCardEndMargin(), mCardConfiguration.getCardBottomMargin());

        mActionButton.setOnClickListener(v -> {
            onClickListener.onClick(v);
            loggingListener.onContentClicked();
        });
        mVisibilityMonitor.setListener(loggingListener);
        setButtonSpinnerVisibility(showSpinner);
    }

    @Override
    public void unbind() {
        // Clear OnClickListener to null to allow for GC.
        mActionButton.setOnClickListener(null);
        mVisibilityMonitor.setListener(null);

        // Set clickable to false as setting OnClickListener to null sets clickable to true.
        mActionButton.setClickable(false);
    }

    public void setShowSpinner(boolean showSpinner) {
        setButtonSpinnerVisibility(/* showSpinner= */ showSpinner);
    }

    private void setButtonSpinnerVisibility(boolean showSpinner) {
        mActionButton.setVisibility(showSpinner ? View.GONE : View.VISIBLE);
        mSpinner.setVisibility(showSpinner ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    VisibilityMonitor createVisibilityMonitor(View view, Configuration configuration) {
        return new VisibilityMonitor(view, configuration);
    }
}
