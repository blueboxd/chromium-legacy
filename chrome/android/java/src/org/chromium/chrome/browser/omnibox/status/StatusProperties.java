// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.util.ObjectsCompat;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Model for the Status view.
 */
class StatusProperties {
    // TODO(wylieb): Investigate the case where we only want to swap the tint (if any).
    /** Encapsulates an icon and tint to allow atomic drawable updates for StatusView. */
    static class StatusIconResource {
        private @DrawableRes Integer mIconRes;
        private @ColorRes int mTint;
        private String mIconIdentifier;
        private Bitmap mBitmap;

        /** Constructor for a custom bitmap. */
        StatusIconResource(String iconIdentifier, Bitmap bitmap, @ColorRes int tint) {
            mIconIdentifier = iconIdentifier;
            mBitmap = bitmap;
            mTint = tint;
        }

        /** Constructor for an Android resource. */
        StatusIconResource(@DrawableRes int iconRes, @ColorRes int tint) {
            mIconRes = iconRes;
            mTint = tint;
        }

        /** @return The tint associated with this resource. */
        @ColorRes
        int getTint() {
            return mTint;
        }

        /** @return The icon res. */
        @DrawableRes
        int getIconResForTesting() {
            if (mIconRes == null) return 0;
            return mIconRes;
        }

        /** @return The {@link Drawable} for this StatusIconResource. */
        Drawable getDrawable(Context context, Resources resources) {
            if (mBitmap != null) {
                Drawable drawable = new BitmapDrawable(resources, mBitmap);
                if (mTint != 0) {
                    DrawableCompat.setTintList(
                            drawable, AppCompatResources.getColorStateList(context, mTint));
                }
                return drawable;
            } else if (mIconRes != null) {
                if (mTint == 0) {
                    return AppCompatResources.getDrawable(context, mIconRes);
                }
                return UiUtils.getTintedDrawable(context, mIconRes, mTint);
            } else {
                return null;
            }
        }

        /** @return The icon identifier, used for testing. */
        @Nullable
        String getIconIdentifierForTesting() {
            return mIconIdentifier;
        }

        @Override
        public boolean equals(@Nullable Object other) {
            if (!(other instanceof StatusIconResource)) return false;

            StatusIconResource otherResource = (StatusIconResource) other;
            if (mTint != otherResource.mTint) return false;
            if (!ObjectsCompat.equals(mIconRes, otherResource.mIconRes)) return false;
            if (mBitmap != null) return mBitmap == otherResource.mBitmap;

            return true;
        }
    }

    /** Enables / disables animations. */
    static final WritableBooleanPropertyKey ANIMATIONS_ENABLED = new WritableBooleanPropertyKey();

    /** Specifies the icon. */
    static final WritableObjectPropertyKey<StatusIconResource> STATUS_ICON_RESOURCE =
            new WritableObjectPropertyKey<>();

    /** Specifies the icon alpha. */
    static final WritableFloatPropertyKey STATUS_ICON_ALPHA = new WritableFloatPropertyKey();

    /** Specifies if the icon should be shown or not. */
    static final WritableBooleanPropertyKey SHOW_STATUS_ICON = new WritableBooleanPropertyKey();

    /** Specifies accessibility string presented to user upon long click on security icon. */
    public static final WritableIntPropertyKey STATUS_ICON_ACCESSIBILITY_TOAST_RES =
            new WritableIntPropertyKey();

    /** Specifies string resource holding content description for security icon. */
    static final WritableIntPropertyKey STATUS_ICON_DESCRIPTION_RES = new WritableIntPropertyKey();

    /** Specifies status separator color. */
    static final WritableIntPropertyKey SEPARATOR_COLOR_RES = new WritableIntPropertyKey();

    /** Specifies object to receive status click events. */
    static final WritableObjectPropertyKey<View.OnClickListener> STATUS_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_COLOR_RES =
            new WritableIntPropertyKey();

    /** Specifies content of the verbose status text field. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_STRING_RES =
            new WritableIntPropertyKey();

    /** Specifies whether verbose status text view is visible. */
    static final WritableBooleanPropertyKey VERBOSE_STATUS_TEXT_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Specifies width of the verbose status text view. */
    static final WritableIntPropertyKey VERBOSE_STATUS_TEXT_WIDTH = new WritableIntPropertyKey();

    /** Specifies whether the incognito badge is visible or not. */
    static final WritableBooleanPropertyKey INCOGNITO_BADGE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {ANIMATIONS_ENABLED,
            STATUS_ICON_ACCESSIBILITY_TOAST_RES, STATUS_ICON_RESOURCE, STATUS_ICON_ALPHA,
            SHOW_STATUS_ICON, STATUS_ICON_DESCRIPTION_RES, SEPARATOR_COLOR_RES,
            STATUS_CLICK_LISTENER, VERBOSE_STATUS_TEXT_COLOR_RES, VERBOSE_STATUS_TEXT_STRING_RES,
            VERBOSE_STATUS_TEXT_VISIBLE, VERBOSE_STATUS_TEXT_WIDTH, INCOGNITO_BADGE_VISIBLE};

    private StatusProperties() {}
}
