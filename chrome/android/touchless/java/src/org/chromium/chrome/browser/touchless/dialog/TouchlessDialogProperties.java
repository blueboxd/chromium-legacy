// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.dialog;

import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.support.annotation.StringRes;
import android.text.TextUtils;
import android.view.InputEvent;
import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The properties that define a touchless dialog. */
public class TouchlessDialogProperties {
    /** Possible list item types for the list of options in the dialog. */
    @IntDef({ListItemType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ListItemType {
        int DEFAULT = 0;
    }

    /** Possible priorities for this set of properties. */
    @IntDef({Priority.HIGH, Priority.MEDIUM, Priority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Priority {
        int HIGH = 0;
        int MEDIUM = 1;
        int LOW = 2;
    }

    /**
     * A click listener for dialog actions.
     */
    public interface OnClickListener {
        /**
         * Called when a view has been clicked.
         *
         * @param event The input event that triggered the click.
         */
        void onClick(InputEvent event);
    }

    /** Properties that determine how list items are displayed in the touchless dialog. */
    public static class DialogListItemProperties {
        /** The icon of the dialog. */
        public static final WritableObjectPropertyKey<Drawable> ICON =
                new WritableObjectPropertyKey<>();

        /** The text shown for the list item. */
        public static final WritableObjectPropertyKey<String> TEXT =
                new WritableObjectPropertyKey<>();

        /** Whether this item can be clicked more than one time. */
        public static final WritableBooleanPropertyKey MULTI_CLICKABLE =
                new WritableBooleanPropertyKey();

        /** The action to be performed when the item is selected. */
        public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
                new WritableObjectPropertyKey<>();

        public static final PropertyKey[] ALL_KEYS = {ICON, TEXT, MULTI_CLICKABLE, CLICK_LISTENER};
    }

    /**
     * Struct-like class for holding the Names for the dialog actions.
     */
    public static class ActionNames {
        public @StringRes int cancel;
        public @StringRes int select;
        public @StringRes int alt;
    }

    /**
     * Whether the dialog is fullscreen. If false there will be a gap at the top showing the content
     * behind the dialog.
     */
    public static final ReadableBooleanPropertyKey IS_FULLSCREEN = new ReadableBooleanPropertyKey();

    /**
     * The list of options to be displayed in the dialog. These models should be using the
     * {@link DialogListItemProperties} properties.
     */
    public static final WritableObjectPropertyKey<PropertyModel[]> LIST_MODELS =
            new WritableObjectPropertyKey<>();

    /** The names for the Actions. */
    public static final WritableObjectPropertyKey<ActionNames> ACTION_NAMES =
            new WritableObjectPropertyKey<>();

    /** What will happen when cancel is triggered. */
    public static final WritableObjectPropertyKey<OnClickListener> CANCEL_ACTION =
            new WritableObjectPropertyKey<>();

    /** Do not use, this is in the process of being deleted. */
    @Deprecated
    public static final WritableBooleanPropertyKey IS_CANCEL_ACTION_EXTERNAL =
            new WritableBooleanPropertyKey();

    /** What will happen when alternative action is triggered. */
    public static final WritableObjectPropertyKey<OnClickListener> ALT_ACTION =
            new WritableObjectPropertyKey<>();

    /** Do not use, this is in the process of being deleted. */
    @Deprecated
    public static final WritableBooleanPropertyKey IS_ALT_ACTION_EXTERNAL =
            new WritableBooleanPropertyKey();

    /** The priority for this set of properties. */
    public static final WritableIntPropertyKey PRIORITY = new WritableIntPropertyKey();

    /** Force the title to be a single line and truncate with an ellipsis. */
    public static final WritableBooleanPropertyKey FORCE_SINGLE_LINE_TITLE =
            new WritableBooleanPropertyKey();

    /** Force the title to have a specific text direction. See {@link View#setTextDirection}. */
    public static final WritableIntPropertyKey TITLE_DIRECTION = new WritableIntPropertyKey();

    /** Set ellipsize location for the title. See {@link TextView#setEllipsize}. */
    public static final WritableObjectPropertyKey<TextUtils.TruncateAt> TITLE_ELLIPSIZE =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] MINIMAL_DIALOG_KEYS = {
            ModalDialogProperties.TITLE, ACTION_NAMES, CANCEL_ACTION, ALT_ACTION, PRIORITY};

    public static final PropertyKey[] ALL_DIALOG_KEYS = PropertyModel.concatKeys(
            ModalDialogProperties.ALL_KEYS,
            new PropertyKey[] {ACTION_NAMES, CANCEL_ACTION, ALT_ACTION, PRIORITY, IS_FULLSCREEN,
                    LIST_MODELS, FORCE_SINGLE_LINE_TITLE, TITLE_DIRECTION, TITLE_ELLIPSIZE});
}
