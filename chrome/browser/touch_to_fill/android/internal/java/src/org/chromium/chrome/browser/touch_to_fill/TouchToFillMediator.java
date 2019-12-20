// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FIELD_TRIAL_PARAM_BRANDING_MESSAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FIELD_TRIAL_PARAM_SHOW_CONFIRMATION_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.BRANDING_MESSAGE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SINGLE_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import androidx.annotation.Px;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillComponent.UserAction;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the TouchToFill component. It sets the state of the model and reacts to
 * events like clicks.
 */
class TouchToFillMediator {
    static final String UMA_TOUCH_TO_FILL_DISMISSAL_REASON =
            "PasswordManager.TouchToFill.DismissalReason";
    static final String UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX =
            "PasswordManager.TouchToFill.CredentialIndex";
    static final String UMA_TOUCH_TO_FILL_USER_ACTION = "PasswordManager.TouchToFill.UserAction";

    private TouchToFillComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private LargeIconBridge mLargeIconBridge;
    private @Px int mDesiredIconSize;
    private List<Credential> mCredentials;

    void initialize(TouchToFillComponent.Delegate delegate, PropertyModel model,
            LargeIconBridge largeIconBridge, @Px int desiredIconSize) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mLargeIconBridge = largeIconBridge;
        mDesiredIconSize = desiredIconSize;
    }

    void showCredentials(String url, boolean isOriginSecure, List<Credential> credentials) {
        assert credentials != null;
        mModel.set(ON_CLICK_MANAGE, this::onManagePasswordSelected);

        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(new ListItem(TouchToFillProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(SINGLE_CREDENTIAL, credentials.size() == 1)
                        .with(FORMATTED_URL,
                                UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url))
                        .with(ORIGIN_SECURE, isOriginSecure)
                        .build()));

        mCredentials = credentials;
        for (Credential credential : credentials) {
            final PropertyModel model = createModel(credential);
            sheetItems.add(new ListItem(TouchToFillProperties.ItemType.CREDENTIAL, model));
            requestIconOrFallbackImage(model, url);
            if (shouldCreateConfirmationButton(credentials)) {
                sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FILL_BUTTON, model));
            }
        }

        sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FOOTER,
                new PropertyModel.Builder(TouchToFillProperties.FooterProperties.ALL_KEYS)
                        .with(BRANDING_MESSAGE_ID, getBrandingMessageId())
                        .build()));

        mModel.set(VISIBLE, true);
    }

    private void requestIconOrFallbackImage(PropertyModel credentialModel, String url) {
        Credential credential = credentialModel.get(CREDENTIAL);
        final String iconOrigin = getIconOrigin(credential.getOriginUrl(), url);

        final LargeIconCallback setIcon = (icon, fallbackColor, hasDefaultColor, type) -> {
            credentialModel.set(FAVICON_OR_FALLBACK,
                    new CredentialProperties.FaviconOrFallback(iconOrigin, icon, fallbackColor,
                            hasDefaultColor, type, mDesiredIconSize));
        };
        final LargeIconCallback setIconOrRetry = (icon, fallbackColor, hasDefaultColor, type) -> {
            if (icon == null && iconOrigin.equals(credential.getOriginUrl())) {
                mLargeIconBridge.getLargeIconForUrl(url, mDesiredIconSize, setIcon);
                return; // Unlikely but retry for exact path if there is no icon for the origin.
            }
            setIcon.onLargeIconAvailable(icon, fallbackColor, hasDefaultColor, type);
        };
        mLargeIconBridge.getLargeIconForUrl(iconOrigin, mDesiredIconSize, setIconOrRetry);
    }

    private String getIconOrigin(String credentialOrigin, String siteUrl) {
        final Origin o = Origin.create(credentialOrigin);
        // TODO(crbug.com/1030230): assert o != null as soon as credential Origin must be valid.
        return o != null && !o.uri().isOpaque() ? credentialOrigin : siteUrl;
    }

    private void onSelectedCredential(Credential credential) {
        mModel.set(VISIBLE, false);
        if (mCredentials.size() > 1) {
            // We only record this histogram in case multiple credentials were shown to the user.
            // Otherwise the single credential case where position should always be 0 will dominate
            // the recording.
            RecordHistogram.recordCount100Histogram(
                    UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX, mCredentials.indexOf(credential));
        }

        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_USER_ACTION,
                UserAction.SELECT_CREDENTIAL, UserAction.MAX_VALUE + 1);
        mDelegate.onCredentialSelected(credential);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_DISMISSAL_REASON, reason,
                BottomSheetController.StateChangeReason.MAX_VALUE + 1);
        RecordHistogram.recordEnumeratedHistogram(
                UMA_TOUCH_TO_FILL_USER_ACTION, UserAction.DISMISS, UserAction.MAX_VALUE + 1);
        mDelegate.onDismissed();
    }

    private void onManagePasswordSelected() {
        mModel.set(VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_USER_ACTION,
                UserAction.SELECT_MANAGE_PASSWORDS, UserAction.MAX_VALUE + 1);
        mDelegate.onManagePasswordsSelected();
    }

    /**
     * @param credentials The available credentials. Show the confirmation for a lone credential.
     * @return True if a confirmation button should be shown at the end of the bottom sheet.
     */
    private boolean shouldCreateConfirmationButton(List<Credential> credentials) {
        return credentials.size() == 1
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.TOUCH_TO_FILL_ANDROID,
                        FIELD_TRIAL_PARAM_SHOW_CONFIRMATION_BUTTON, false);
    }

    /**
     * Returns the id of the branding message to be shown. Returns 0 in case no message should be
     * displayed.
     */
    private int getBrandingMessageId() {
        int id = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.TOUCH_TO_FILL_ANDROID, FIELD_TRIAL_PARAM_BRANDING_MESSAGE, 0);
        switch (id) {
            case 1:
                return R.string.touch_to_fill_branding_variation_1;
            case 2:
                return R.string.touch_to_fill_branding_variation_2;
            case 3:
                return R.string.touch_to_fill_branding_variation_3;
            default:
                return 0;
        }
    }

    private PropertyModel createModel(Credential credential) {
        return new PropertyModel.Builder(CredentialProperties.ALL_KEYS)
                .with(CREDENTIAL, credential)
                .with(ON_CLICK_LISTENER, this::onSelectedCredential)
                .with(FORMATTED_ORIGIN,
                        UrlFormatter.formatUrlForDisplayOmitScheme(credential.getOriginUrl()))
                .build();
    }
}
