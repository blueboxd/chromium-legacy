// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.chrome.browser.widget.AlertDialogEditText;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** A touchless variation of the {@link AddToHomescreenDialog}. */
public class TouchlessAddToHomescreenDialog extends AddToHomescreenDialog {
    private Activity mActivity;
    private Delegate mDelegate;
    private ModalDialogManager mDialogManager;
    private PropertyModel mModel;

    public TouchlessAddToHomescreenDialog(
            Activity activity, ModalDialogManager dialogManager, Delegate delegate) {
        super(activity, delegate);
        mActivity = activity;
        mDialogManager = dialogManager;
        mDelegate = delegate;
    }

    @Override
    public void show() {
        mModel = buildTouchlessDialogModel();
        mDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Build the property model for the dialog in touchless mode.
     * @return A model to pass to a {@link ModalDialogManager}.
     */
    private PropertyModel buildTouchlessDialogModel() {
        Resources res = mActivity.getResources();
        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                mDelegate.onDialogDismissed();
            }
        };
        final PropertyModel model =
                new PropertyModel.Builder(TouchlessDialogProperties.ALL_DIALOG_KEYS)
                        .with(TouchlessDialogProperties.IS_FULLSCREEN, false)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .build();
        model.set(TouchlessDialogProperties.PRIORITY, TouchlessDialogProperties.Priority.HIGH);
        model.set(ModalDialogProperties.TITLE,
                res.getString(org.chromium.chrome.R.string.menu_add_to_apps));
        TouchlessDialogProperties.ActionNames actions = new TouchlessDialogProperties.ActionNames();
        actions.cancel = org.chromium.chrome.R.string.cancel;
        actions.select = org.chromium.chrome.R.string.select;
        actions.alt = 0;
        model.set(TouchlessDialogProperties.ACTION_NAMES, actions);

        model.set(TouchlessDialogProperties.CANCEL_ACTION,
                (v) -> mDialogManager.dismissDialog(model, DialogDismissalCause.UNKNOWN));

        model.set(TouchlessDialogProperties.LIST_MODELS,
                new PropertyModel[] {buildListItemModel("")});

        model.set(ModalDialogProperties.CUSTOM_VIEW,
                mActivity.getLayoutInflater().inflate(R.layout.touchless_add_to_apps, null));

        return model;
    }

    /**
     * Build the list item that adds the app or site to the home screen.
     * @param title The title of the app or site.
     * @return The list item for the dialog model.
     */
    private PropertyModel buildListItemModel(final String title) {
        Resources res = mActivity.getResources();
        return new PropertyModel.Builder(DialogListItemProperties.ALL_KEYS)
                .with(DialogListItemProperties.TEXT,
                        res.getString(org.chromium.chrome.R.string.add))
                .with(DialogListItemProperties.ICON,
                        ApiCompatibilityUtils.getDrawable(
                                res, org.chromium.chrome.R.drawable.ic_add))
                .with(DialogListItemProperties.MULTI_CLICKABLE, false)
                .with(DialogListItemProperties.CLICK_LISTENER,
                        (v) -> {
                            ViewGroup group =
                                    (ViewGroup) mModel.get(ModalDialogProperties.CUSTOM_VIEW);
                            String customTitle =
                                    ((AlertDialogEditText) group.findViewById(R.id.app_title))
                                            .getText()
                                            .toString();
                            if (TextUtils.isEmpty(customTitle)) customTitle = title;

                            if (TextUtils.isEmpty(customTitle)) return;
                            mDelegate.addToHomescreen(customTitle);
                            mDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
                        })
                .build();
    }

    /**
     * Update the custom view shown for the touchless dialog. This shows the name of the app, icon,
     * and site.
     */
    private void updateModelCustomView(Bitmap icon, String title, String origin) {
        ViewGroup group = (ViewGroup) mModel.get(ModalDialogProperties.CUSTOM_VIEW);

        group.findViewById(R.id.spinny).setVisibility(View.GONE);
        group.findViewById(R.id.icon).setVisibility(View.VISIBLE);
        group.findViewById(R.id.app_title).setVisibility(View.VISIBLE);

        if (icon != null) ((ImageView) group.findViewById(R.id.icon)).setImageBitmap(icon);
        if (title != null) {
            ((AlertDialogEditText) group.findViewById(R.id.app_title)).setText(title);
        }
    }

    @Override
    public void onUserTitleAvailable(String title, String url, boolean isWebapp) {
        updateModelCustomView(null, title, url);
        mModel.set(TouchlessDialogProperties.LIST_MODELS,
                new PropertyModel[] {buildListItemModel(title)});
    }

    @Override
    public void onUserTitleAvailable(String title, String installText, float rating) {
        updateModelCustomView(null, title, null);
    }

    @Override
    public void onIconAvailable(Bitmap icon) {
        updateModelCustomView(icon, null, null);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.O)
    public void onAdaptableIconAvailable(Bitmap icon) {
        updateModelCustomView(icon, null, null);
    }

    @Override
    public void onClick(View v) {
        // Intentionally do nothing.
    }
}
