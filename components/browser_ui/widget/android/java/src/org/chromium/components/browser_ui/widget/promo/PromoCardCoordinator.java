// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * PromoCard Coordinator that owns the view and the model change processor. Client will need to
 * create another layer of controller to own this coordinator, and pass in the {@link PropertyModel}
 * to initialize the view.
 */
public class PromoCardCoordinator {
    private PromoCardView mPromoCardView;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private String mFeatureName;

    /**
     * Create the Coordinator of PromoCard that owns the view and the change process.
     * @param context Context used to create the view.
     * @param model {@link PropertyModel} built with {@link PromoCardProperties}.
     * @param featureName Name of the feature of this promo. Will be used to create keys for
     *         SharedPreference.
     */
    public PromoCardCoordinator(Context context, PropertyModel model, String featureName) {
        mPromoCardView = (PromoCardView) LayoutInflater.from(context).inflate(
                R.layout.promo_card_view, null, false);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, mPromoCardView, new PromoCardViewBinder());
        mFeatureName = featureName;
    }

    /**
     * Destroy the PromoCard component and release the PropertyModelChangeProcessor.
     */
    public void destroy() {
        mModelChangeProcessor.destroy();
    }

    /**
     * @return {@link PromoCardView} held by this promo component.
     */
    public View getView() {
        return mPromoCardView;
    }

    /**
     * @return Name of the feature this promo is representing.
     */
    public String getFeatureName() {
        return mFeatureName;
    }
}
