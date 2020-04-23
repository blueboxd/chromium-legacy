// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the fake search box component,
 * and updating the model accordingly.
 */
public class SearchBoxCoordinator {
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private final SearchBoxMediator mMediator;

    /** Constructor. */
    public SearchBoxCoordinator(Context context, ViewGroup parent) {
        mModel = new PropertyModel(SearchBoxProperties.ALL_KEYS);
        mView = parent.findViewById(R.id.search_box);
        mMediator = new SearchBoxMediator(context, mModel, mView);
    }

    public void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mMediator.initialize(activityLifecycleDispatcher);
    }

    public View getView() {
        return mView;
    }

    public void destroy() {
        mMediator.destroy();
    }

    public void setAlpha(float alpha) {
        mModel.set(SearchBoxProperties.ALPHA, alpha);
    }

    public void setBackground(Drawable background) {
        mModel.set(SearchBoxProperties.BACKGROUND, background);
    }

    public void setVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VISIBILITY, visible);
    }

    public void setSearchText(String text) {
        mModel.set(SearchBoxProperties.SEARCH_TEXT, text);
    }

    public void setSearchBoxClickListener(OnClickListener listener) {
        mMediator.setSearchBoxClickListener(listener);
    }

    public void setSearchBoxTextWatcher(TextWatcher textWatcher) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, textWatcher);
    }

    public void setSearchBoxHintColor(int hintTextColor) {
        mModel.set(SearchBoxProperties.SEARCH_BOX_HINT_COLOR, hintTextColor);
    }

    public void setVoiceSearchButtonVisibility(boolean visible) {
        mModel.set(SearchBoxProperties.VOICE_SEARCH_VISIBILITY, visible);
    }

    public void setVoiceSearchButtonClickListener(OnClickListener listener) {
        mModel.set(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, listener);
    }

    public void setChipText(String chipText) {
        mMediator.setChipText(chipText);
    }

    public void setChipDelegate(SearchBoxChipDelegate chipDelegate) {
        mMediator.setChipDelegate(chipDelegate);
    }
}
