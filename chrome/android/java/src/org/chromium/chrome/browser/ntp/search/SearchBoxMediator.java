// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;

import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

class SearchBoxMediator
        implements Destroyable, NativeInitObserver, AssistantVoiceSearchService.Observer {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ViewGroup mView;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private AssistantVoiceSearchService mAssistantVoiceSearchService;

    public SearchBoxMediator(Context context, PropertyModel model, ViewGroup view) {
        mContext = context;
        mModel = model;
        mView = view;

        PropertyModelChangeProcessor.create(mModel, mView, new SearchBoxViewBinder());
    }

    /**
     * Initializes the SearchBoxContainerView with the given params. This must be called for
     * classes that use the SearchBoxContainerView.
     *
     * @param activityLifecycleDispatcher Used to register for lifecycle events.
     */
    public void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        assert mActivityLifecycleDispatcher == null;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);
        if (mActivityLifecycleDispatcher.isNativeInitializationFinished()) {
            onFinishNativeInitialization();
        }
    }

    @Override
    public void destroy() {
        if (mAssistantVoiceSearchService != null) {
            mAssistantVoiceSearchService.destroy();
            mAssistantVoiceSearchService = null;
        }

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        mAssistantVoiceSearchService =
                new AssistantVoiceSearchService(mContext, ExternalAuthUtils.getInstance(),
                        TemplateUrlServiceFactory.get(), GSAState.getInstance(mContext), this);
        onAssistantVoiceSearchServiceChanged();
    }

    @Override
    public void onAssistantVoiceSearchServiceChanged() {
        // Potential race condition between destroy and the observer, see crbug.com/1055274.
        if (mAssistantVoiceSearchService == null) return;

        Drawable drawable = mAssistantVoiceSearchService.getCurrentMicDrawable();
        mModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, drawable);

        final @ColorRes int primaryColor = ChromeColors.getDefaultThemeColor(
                mContext.getResources(), false /* forceDarkBgColor= */);
        ColorStateList colorStateList =
                mAssistantVoiceSearchService.getMicButtonColorStateList(primaryColor, mContext);
        mModel.set(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST, colorStateList);
    }
}
