// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import android.view.View;

import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.ExecutionException;

/**
 * Utility methods for testing touchless modal dialogs.
 */
public class TouchlessDialogTestUtils {
    public static void showDialog(ModalDialogManager manager, PropertyModel model) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> manager.showDialog(model, ModalDialogManager.ModalDialogType.APP));
    }

    public static PropertyModel createDialog(String title) throws ExecutionException {
        return createDialog(title, false);
    }

    public static PropertyModel createDialog(String title, boolean isFullscreen)
            throws ExecutionException {
        return createDialog(title, isFullscreen, null, null);
    }

    public static PropertyModel createDialog(String title, boolean isFullscreen, String[] items,
            View.OnClickListener[] itemListeners) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {}

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {}
            };

            PropertyModel model =
                    new PropertyModel.Builder(TouchlessDialogProperties.ALL_DIALOG_KEYS)
                            .with(ModalDialogProperties.CONTROLLER, controller)
                            .with(ModalDialogProperties.TITLE, title)
                            .with(TouchlessDialogProperties.IS_FULLSCREEN, isFullscreen)
                            .build();
            if (items != null && itemListeners != null) {
                model.set(
                        TouchlessDialogProperties.LIST_MODELS, generateItems(items, itemListeners));
            }
            return model;
        });
    }

    private static PropertyModel[] generateItems(
            String[] itemTitles, View.OnClickListener[] itemListeners) {
        PropertyModel[] items = new PropertyModel[itemTitles.length];
        for (int i = 0; i < items.length; i++) {
            items[i] = new PropertyModel.Builder(DialogListItemProperties.ALL_KEYS)
                               .with(DialogListItemProperties.TEXT, itemTitles[i])
                               .with(DialogListItemProperties.CLICK_LISTENER, itemListeners[i])
                               .build();
        }
        return items;
    }
}
