// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.basicstream.internal.viewholders;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.support.v7.widget.RecyclerView.LayoutParams;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.VisibilityMonitor;

/** {@link android.support.v7.widget.RecyclerView.ViewHolder} for the more button. */
public class ContinuationViewHolder extends FeedViewHolder {
    private final View actionButton;
    private final View spinner;
    private final VisibilityMonitor visibilityMonitor;
    private final CardConfiguration cardConfiguration;
    private final Context context;

    public ContinuationViewHolder(Configuration configuration, Context context,
            FrameLayout frameLayout, CardConfiguration cardConfiguration) {
        super(frameLayout);
        View containerView =
                LayoutInflater.from(context).inflate(R.layout.feed_more_button, frameLayout);
        actionButton = checkNotNull(frameLayout.findViewById(R.id.action_button));
        spinner = checkNotNull(frameLayout.findViewById(R.id.loading_spinner));
        visibilityMonitor = createVisibilityMonitor(containerView, configuration);
        this.cardConfiguration = cardConfiguration;
        this.context = context;
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
                cardConfiguration.getCardStartMargin(),
                (int) context.getResources().getDimension(
                        R.dimen.feed_more_button_container_top_margins),
                cardConfiguration.getCardEndMargin(), cardConfiguration.getCardBottomMargin());

        actionButton.setOnClickListener(v -> {
            onClickListener.onClick(v);
            loggingListener.onContentClicked();
        });
        visibilityMonitor.setListener(loggingListener);
        setButtonSpinnerVisibility(showSpinner);
    }

    @Override
    public void unbind() {
        // Clear OnClickListener to null to allow for GC.
        actionButton.setOnClickListener(null);
        visibilityMonitor.setListener(null);

        // Set clickable to false as setting OnClickListener to null sets clickable to true.
        actionButton.setClickable(false);
    }

    public void setShowSpinner(boolean showSpinner) {
        setButtonSpinnerVisibility(/* showSpinner= */ showSpinner);
    }

    private void setButtonSpinnerVisibility(boolean showSpinner) {
        actionButton.setVisibility(showSpinner ? View.GONE : View.VISIBLE);
        spinner.setVisibility(showSpinner ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    VisibilityMonitor createVisibilityMonitor(
            /*@UnderInitialization*/ ContinuationViewHolder this, View view,
            Configuration configuration) {
        return new VisibilityMonitor(view, configuration);
    }
}
