// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;

import android.content.Context;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.helper.ListViewAdapter;
import org.chromium.chrome.browser.touch_to_fill.helper.SimpleListViewMcp;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Creates the TouchToFill component. TouchToFill uses a bottom sheet to let the
 * user select a set of credentials and fills it into the focused form.
 */
public class TouchToFillCoordinator implements TouchToFillComponent {
    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel = TouchToFillProperties.createDefaultModel(mMediator);

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillComponent.Delegate delegate) {
        mMediator.initialize(delegate, mModel);
        setUpModelChangeProcessors(mModel, new TouchToFillView(context, sheetController));
    }

    @Override
    public void showCredentials(String formattedUrl, List<Credential> credentials) {
        mMediator.showCredentials(formattedUrl, credentials);
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link TouchToFillProperties}.
     * @param view A {@link TouchToFillView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, TouchToFillView view) {
        PropertyModelChangeProcessor.create(model, view, TouchToFillViewBinder::bind);
        view.setCredentialListAdapter(
                new ListViewAdapter<>(new SimpleListViewMcp<>(model.get(CREDENTIAL_LIST),
                                              TouchToFillViewBinder::bindCredentialView),
                        TouchToFillViewBinder::createCredentialView));
    }
}
