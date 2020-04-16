// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Intent;
import android.text.SpannableString;

import androidx.annotation.IntDef;

import org.chromium.base.Consumer;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.previews.PreviewsUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.site_settings.CookieControlsBridge;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.components.page_info.VrHandler;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Chrome's implementation of PageInfoControllerDelegate, that provides Chrome-specific info to
 * PageInfoController. It also contains logic for Chrome-specific features, like {@link Previews}
 */
public class ChromePageInfoControllerDelegate implements PageInfoControllerDelegate {
    @IntDef({PreviewPageState.NOT_PREVIEW, PreviewPageState.SECURE_PAGE_PREVIEW,
            PreviewPageState.INSECURE_PAGE_PREVIEW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreviewPageState {
        int NOT_PREVIEW = 1;
        int SECURE_PAGE_PREVIEW = 2;
        int INSECURE_PAGE_PREVIEW = 3;
    }

    private final WebContents mWebContents;
    private final ChromeActivity mActivity;
    private final @PreviewPageState int mPreviewPageState;

    public ChromePageInfoControllerDelegate(ChromeActivity activity, WebContents webContents) {
        mWebContents = webContents;
        mActivity = activity;
        mPreviewPageState = getPreviewPageStateAndRecordUma();
    }

    private Profile profile() {
        return Profile.fromWebContents(mWebContents);
    }

    /**
     * Return the state of the webcontents showing the preview.
     */
    private @PreviewPageState int getPreviewPageStateAndRecordUma() {
        final int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
        @PreviewPageState
        int previewPageState = PreviewPageState.NOT_PREVIEW;
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        if (bridge.shouldShowPreviewUI(mWebContents)) {
            previewPageState = securityLevel == ConnectionSecurityLevel.SECURE
                    ? PreviewPageState.SECURE_PAGE_PREVIEW
                    : PreviewPageState.INSECURE_PAGE_PREVIEW;

            PreviewsUma.recordPageInfoOpened(bridge.getPreviewsType(mWebContents));
            TrackerFactory.getTrackerForProfile(profile()).notifyEvent(
                    EventConstants.PREVIEWS_VERBOSE_STATUS_OPENED);
        }
        return previewPageState;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return new ChromeAutocompleteSchemeClassifier(profile());
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean cookieControlsShown() {
        return CookieControlsBridge.isCookieControlsEnabled(profile());
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mActivity.getModalDialogManager();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean useDarkColors() {
        return !mActivity.getNightModeStateProvider().isInNightMode();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void initPreviewUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        final PreviewsAndroidBridge bridge = PreviewsAndroidBridge.getInstance();
        viewParams.previewSeparatorShown =
                mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW;
        viewParams.previewUIShown = isShowingPreview();
        if (isShowingPreview()) {
            viewParams.urlTitleShown = false;
            viewParams.connectionMessageShown = false;

            viewParams.previewShowOriginalClickCallback = () -> {
                runAfterDismiss.accept(() -> {
                    PreviewsUma.recordOptOut(bridge.getPreviewsType(mWebContents));
                    bridge.loadOriginal(mWebContents);
                });
            };
            final String previewOriginalHost =
                    bridge.getOriginalHost(mWebContents.getVisibleUrlString());
            final String loadOriginalText = mActivity.getString(
                    R.string.page_info_preview_load_original, previewOriginalHost);
            final SpannableString loadOriginalSpan = SpanApplier.applySpans(loadOriginalText,
                    new SpanInfo("<link>", "</link>",
                            // The callback given to NoUnderlineClickableSpan is overridden in
                            // PageInfoView so use previewShowOriginalClickCallback (above) instead
                            // because the entire TextView will be clickable.
                            new NoUnderlineClickableSpan(mActivity.getResources(), (view) -> {})));
            viewParams.previewLoadOriginalMessage = loadOriginalSpan;

            viewParams.previewStaleTimestamp = bridge.getStalePreviewTimestamp(mWebContents);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isShowingPreview() {
        return mPreviewPageState != PreviewPageState.NOT_PREVIEW;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isPreviewPageInsecure() {
        return mPreviewPageState == PreviewPageState.INSECURE_PAGE_PREVIEW;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isInstantAppAvailable(String url) {
        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        return instantAppsHandler.isInstantAppAvailable(
                url, false /* checkHoldback */, false /* includeUserPrefersBrowser */);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Intent getInstantAppIntentForUrl(String url) {
        InstantAppsHandler instantAppsHandler = InstantAppsHandler.getInstance();
        return instantAppsHandler.getInstantAppIntentForUrl(url);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public VrHandler getVrHandler() {
        return VrModuleProvider.getDelegate();
    }
}
