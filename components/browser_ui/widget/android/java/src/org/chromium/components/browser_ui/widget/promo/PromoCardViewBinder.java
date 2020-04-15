// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * View binder than binds the promo view with the property model.
 */
class PromoCardViewBinder
        implements PropertyModelChangeProcessor
                           .ViewBinder<PropertyModel, PromoCardView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, PromoCardView view, PropertyKey propertyKey) {
        if (propertyKey == PromoCardProperties.IMAGE) {
            view.mPromoImage.setImageDrawable(model.get(PromoCardProperties.IMAGE));
        } else if (propertyKey == PromoCardProperties.TITLE) {
            view.mTitle.setText(model.get(PromoCardProperties.TITLE));
        } else if (propertyKey == PromoCardProperties.DESCRIPTION) {
            view.mDescription.setText(model.get(PromoCardProperties.DESCRIPTION));
        } else if (propertyKey == PromoCardProperties.PRIMARY_BUTTON_TEXT) {
            view.mPrimaryButton.setText(model.get(PromoCardProperties.PRIMARY_BUTTON_TEXT));
        } else if (propertyKey == PromoCardProperties.SECONDARY_BUTTON_TEXT) {
            view.mSecondaryButton.setText(model.get(PromoCardProperties.SECONDARY_BUTTON_TEXT));

            // Visibility properties
        } else if (propertyKey == PromoCardProperties.SECONDARY_BUTTON_VISIBLE) {
            view.mSecondaryButton.setVisibility(
                    model.get(PromoCardProperties.SECONDARY_BUTTON_VISIBLE) ? View.VISIBLE
                                                                            : View.GONE);

            // Callback properties
        } else if (propertyKey == PromoCardProperties.PRIMARY_BUTTON_CALLBACK) {
            Callback<View> callback = model.get(PromoCardProperties.PRIMARY_BUTTON_CALLBACK);
            view.mPrimaryButton.setOnClickListener(callback::onResult);
        } else if (propertyKey == PromoCardProperties.SECONDARY_BUTTON_CALLBACK) {
            Callback<View> callback = model.get(PromoCardProperties.SECONDARY_BUTTON_CALLBACK);
            view.mSecondaryButton.setOnClickListener(callback::onResult);
        } else if (propertyKey == PromoCardProperties.CLOSE_BUTTON_CALLBACK) {
            Callback<View> callback = model.get(PromoCardProperties.CLOSE_BUTTON_CALLBACK);
            view.mDismissButton.setOnClickListener(callback::onResult);
        }
    }
}
