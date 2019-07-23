// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import android.app.Dialog;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.util.Pair;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.touchless.TouchlessModelCoordinator;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.DialogListItemProperties;
import org.chromium.chrome.browser.touchless.dialog.TouchlessDialogProperties.ListItemType;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.Presenter;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;

/** A modal dialog presenter that is specific to touchless dialogs. */
public class TouchlessDialogPresenter extends Presenter {
    /** An activity to attach dialogs to. */
    private final ChromeActivity mActivity;
    private final TouchlessModelCoordinator mModelCoordinator;

    /** The dialog this class abstracts. */
    private Dialog mDialog;

    /** The model change processor for the currently shown dialog. */
    private PropertyModelChangeProcessor<PropertyModel, Pair<ViewGroup, ModelListAdapter>,
            PropertyKey> mModelChangeProcessor;

    public TouchlessDialogPresenter(
            ChromeActivity activity, TouchlessModelCoordinator modelCoordinator) {
        mActivity = activity;
        mModelCoordinator = modelCoordinator;
    }

    @Override
    protected void addDialogView(PropertyModel model) {
        // If the activity's decor view is not attached to window, we don't show the dialog because
        // the window manager might have revoked the window token for this activity. See
        // https://crbug.com/926688.
        Window window = mActivity.getWindow();
        if (window == null || !ViewCompat.isAttachedToWindow(window.getDecorView())) {
            dismissCurrentDialog(DialogDismissalCause.NOT_ATTACHED_TO_WINDOW);
            return;
        }

        boolean fullscreen = isFullscreen(model);

        // If not fullscreen don't use a predefined style.
        if (!fullscreen) {
            mDialog = new Dialog(mActivity);
            mDialog.getWindow().setGravity(Gravity.BOTTOM);
            mDialog.getWindow().setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                    mActivity.getResources(), R.drawable.dialog_rounded_top));
        } else {
            mDialog = new Dialog(mActivity, R.style.Theme_Chromium_DialogWhenLarge);
        }

        mDialog.setOnCancelListener(dialogInterface
                -> dismissCurrentDialog(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE));
        mDialog.setOnShowListener(dialog -> {
            if (mModelCoordinator != null) mModelCoordinator.addModelToQueue(model);
        });
        mDialog.setOnDismissListener(dialog -> {
            if (mModelCoordinator != null) mModelCoordinator.removeModelFromQueue(model);
        });
        // Cancel on touch outside should be disabled by default. The ModelChangeProcessor wouldn't
        // notify change if the property is not set during initialization.
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.setOnKeyListener((dialog, keyCode, event) -> {
            return mModelCoordinator != null ? (mModelCoordinator.onKeyEvent(event) == null)
                                             : false;
        });
        ViewGroup dialogView = (ViewGroup) LayoutInflater.from(mDialog.getContext())
                .inflate(R.layout.touchless_dialog_view, null);
        ModelListAdapter adapter = new ModelListAdapter();
        adapter.registerType(ListItemType.DEFAULT,
                () -> LayoutInflater.from(mActivity).inflate(R.layout.dialog_list_item, null),
                TouchlessDialogPresenter::bindListItem);
        ListView dialogOptions = dialogView.findViewById(R.id.touchless_dialog_option_list);
        dialogOptions.setAdapter(adapter);
        dialogOptions.setItemsCanFocus(true);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, Pair.create(dialogView, adapter), TouchlessDialogPresenter::bind);
        mDialog.setContentView(dialogView);

        // If the modal dialog is not specified to be fullscreen, wrap content and place at the
        // bottom of the screen. This needs to be done after content is added to the dialog.
        if (!fullscreen) {
            mDialog.getWindow().setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        mDialog.show();
        dialogView.announceForAccessibility(getContentDescription(model));
    }

    private boolean isFullscreen(PropertyModel model) {
        if (!model.getAllSetProperties().contains(TouchlessDialogProperties.IS_FULLSCREEN)) {
            // We should have this property set. This code is meant to guard against cases
            // where we've forgotten to add this property.
            assert false;
            return false;
        }
        return model.get(TouchlessDialogProperties.IS_FULLSCREEN);
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
            mModelChangeProcessor = null;
        }

        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

    /**
     * Bind a model to a touchless dialog view.
     * @param model The model being bound.
     * @param view The view to apply the model to.
     * @param propertyKey The property that changed.
     */
    private static void bind(
            PropertyModel model, Pair<ViewGroup, ModelListAdapter> view, PropertyKey propertyKey) {
        ViewGroup dialogView = view.first;
        ModelListAdapter optionsAdapter = view.second;
        assert !model.get(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY);
        assert propertyKey != ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY;
        // TODO(mdjones): If the default buttons are used assert no list items and convert the
        //                buttons to list items.
        if (TouchlessDialogProperties.IS_FULLSCREEN == propertyKey) {
            // TODO(mdjones): Implement fullscreen/non-fullscreen modes.
        } else if (ModalDialogProperties.TITLE_ICON == propertyKey) {
            ChromeImageView imageView = dialogView.findViewById(R.id.touchless_dialog_icon);
            imageView.setImageDrawable(model.get(ModalDialogProperties.TITLE_ICON));
            imageView.setVisibility(View.VISIBLE);
        } else if (ModalDialogProperties.TITLE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_title);
            textView.setText(model.get(ModalDialogProperties.TITLE));
            textView.setVisibility(View.VISIBLE);
        } else if (ModalDialogProperties.MESSAGE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_description);
            textView.setText(model.get(ModalDialogProperties.MESSAGE));
            textView.setVisibility(View.VISIBLE);
        } else if (ModalDialogProperties.CUSTOM_VIEW == propertyKey) {
            ViewGroup customGroup = dialogView.findViewById(R.id.custom);
            customGroup.addView(model.get(ModalDialogProperties.CUSTOM_VIEW));
            customGroup.setVisibility(View.VISIBLE);
        } else if (TouchlessDialogProperties.LIST_MODELS == propertyKey) {
            PropertyModel[] models = model.get(TouchlessDialogProperties.LIST_MODELS);
            ArrayList<Pair<Integer, PropertyModel>> modelPairs = new ArrayList<>();
            for (int i = 0; i < models.length; i++) {
                modelPairs.add(Pair.create(0, models[i]));
            }
            optionsAdapter.updateModels(modelPairs);
        } else if (TouchlessDialogProperties.FORCE_SINGLE_LINE_TITLE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_title);
            textView.setMaxLines(model.get(TouchlessDialogProperties.FORCE_SINGLE_LINE_TITLE)
                            ? 1
                            : Integer.MAX_VALUE);
        } else if (TouchlessDialogProperties.TITLE_DIRECTION == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_title);
            textView.setTextDirection(model.get(TouchlessDialogProperties.TITLE_DIRECTION));
        } else if (TouchlessDialogProperties.TITLE_ELLIPSIZE == propertyKey) {
            TextView textView = dialogView.findViewById(R.id.touchless_dialog_title);
            textView.setEllipsize(model.get(TouchlessDialogProperties.TITLE_ELLIPSIZE));
        }
    }

    /**
     * Bind a list model item to its view.
     * @param model The model to bind.
     * @param view The view to be bound to.
     * @param propertyKey The property that changed.
     */
    private static void bindListItem(PropertyModel model, View view, PropertyKey propertyKey) {
        ChromeImageView imageView = view.findViewById(R.id.dialog_item_icon);
        TextView textView = view.findViewById(R.id.dialog_item_text);
        if (DialogListItemProperties.ICON == propertyKey) {
            if (model.get(DialogListItemProperties.ICON) == null) {
                imageView.setImageDrawable(null);
            } else {
                imageView.setVisibility(View.VISIBLE);
                Drawable icon = model.get(DialogListItemProperties.ICON).mutate();
                icon.clearColorFilter();
                imageView.setImageDrawable(icon);
            }
        } else if (DialogListItemProperties.TEXT == propertyKey) {
            textView.setText(model.get(DialogListItemProperties.TEXT));
        } else if (DialogListItemProperties.CLICK_LISTENER == propertyKey) {
            if (!(model.get(DialogListItemProperties.CLICK_LISTENER)
                                instanceof ClickThrottlingListener)) {
                ClickThrottlingListener listener = new ClickThrottlingListener(
                        model.get(DialogListItemProperties.CLICK_LISTENER));
                listener.setIsMultiClickable(model.get(DialogListItemProperties.MULTI_CLICKABLE));
                model.set(DialogListItemProperties.CLICK_LISTENER, listener);
                view.setOnClickListener(listener);
            } else {
                ClickThrottlingListener listener = (ClickThrottlingListener) model.get(
                        DialogListItemProperties.CLICK_LISTENER);
                listener.resetWasClicked();
                view.setOnClickListener(listener);
            }
        } else if (DialogListItemProperties.MULTI_CLICKABLE == propertyKey) {
            View.OnClickListener listener = model.get(DialogListItemProperties.CLICK_LISTENER);
            if (listener instanceof ClickThrottlingListener) {
                ((ClickThrottlingListener) listener)
                        .setIsMultiClickable(model.get(DialogListItemProperties.MULTI_CLICKABLE));
            }
        }
    }

    /**
     * This OnClickListener implementation ensures that
     * {@link DialogListItemProperties.CLICK_LISTENER} is only called once for models with
     * {@link DialogListItemProperties.MULTI_CLICKABLE} set to false.
     */
    private static class ClickThrottlingListener implements View.OnClickListener {
        private View.OnClickListener mOnClickListener;
        private boolean mWasClicked;
        private boolean mIsMultiClickable;

        private ClickThrottlingListener(View.OnClickListener onClickListener) {
            mOnClickListener = onClickListener;
        }

        private void setIsMultiClickable(boolean isMultiClickable) {
            mIsMultiClickable = isMultiClickable;
        }

        private void resetWasClicked() {
            mWasClicked = false;
        }

        @Override
        public void onClick(View view) {
            if (mOnClickListener == null) return;
            if (!mIsMultiClickable && mWasClicked) return;

            mOnClickListener.onClick(view);
            mWasClicked = true;
        }
    }
}
