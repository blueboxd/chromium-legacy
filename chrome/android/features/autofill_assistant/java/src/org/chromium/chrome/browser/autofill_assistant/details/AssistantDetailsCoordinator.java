// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.content.Context;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsViewBinder.ViewHolder;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Locale;

/**
 * Coordinator responsible for showing details.
 */
public class AssistantDetailsCoordinator {
    private final View mView;
    private final AssistantDetailsModel mModel;

    public AssistantDetailsCoordinator(Context context, AssistantDetailsModel model) {
        this(context,
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                        ? context.getResources().getConfiguration().getLocales().get(0)
                        : context.getResources().getConfiguration().locale,
                model, ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY));
    }

    @VisibleForTesting
    public AssistantDetailsCoordinator(Context context, Locale locale, AssistantDetailsModel model,
            ImageFetcher imageFetcher) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_details, /* root= */ null);
        mModel = model;
        ViewHolder viewHolder = new ViewHolder(context, mView);
        AssistantDetailsViewBinder viewBinder =
                new AssistantDetailsViewBinder(context, locale, imageFetcher);
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);

        // Details view is initially hidden.
        updateVisibility();

        // Observe details in model to hide or show this coordinator view.
        model.addObserver((source, propertyKey) -> {
            if (AssistantDetailsModel.DETAILS == propertyKey) {
                updateVisibility();
            }
        });
    }

    /**
     * Return the view associated to the details.
     */
    public View getView() {
        return mView;
    }

    /**
     * Show or hide the details within its parent and call the {@code mOnVisibilityChanged}
     * listener.
     */
    private void updateVisibility() {
        int visibility =
                mModel.get(AssistantDetailsModel.DETAILS) != null ? View.VISIBLE : View.GONE;
        if (mView.getVisibility() != visibility) {
            mView.setVisibility(visibility);
        }
    }
}
