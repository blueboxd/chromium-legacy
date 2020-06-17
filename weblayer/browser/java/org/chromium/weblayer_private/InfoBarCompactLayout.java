// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.components.infobars.InfoBarInteractionHandler;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.infobars.InfoBarMessageView;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * Lays out controls along a line, sandwiched between an (optional) icon and close button.
 * This should only be used by the {@link InfoBar} class, and is created when the InfoBar subclass
 * declares itself to be using a compact layout via {@link InfoBar#usesCompactLayout}.
 */
public class InfoBarCompactLayout extends LinearLayout implements View.OnClickListener {
    private final InfoBarInteractionHandler mInfoBar;
    private final int mCompactInfoBarSize;
    private final int mIconWidth;
    private final View mCloseButton;

    /**
     * Constructs a compat layout for the specified infobar.
     * @param context The context used to render.
     * @param infoBar {@link InfoBarInteractionHandler} that listens to events.
     * @param iconResourceId Resource ID of the icon to use for the infobar.
     * @param iconTintId The {@link ColorRes} used as tint for {@code iconResourceId}.
     * @param iconBitmap Bitmap for the icon to use, if {@code iconResourceId} is not set.
     */
    // TODO(crbug/1056346): ctor is made public to allow access from InfoBar. Once
    // InfoBar is modularized, restore access to package private.
    public InfoBarCompactLayout(Context context, InfoBarInteractionHandler infoBar,
            int iconResourceId, @ColorRes int iconTintId, Bitmap iconBitmap) {
        super(context);
        mInfoBar = infoBar;
        mCompactInfoBarSize =
                context.getResources().getDimensionPixelOffset(R.dimen.infobar_compact_size);
        mIconWidth = context.getResources().getDimensionPixelOffset(R.dimen.infobar_big_icon_size);

        setOrientation(LinearLayout.HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);

        prepareIcon(iconResourceId, iconTintId, iconBitmap);
        mCloseButton = prepareCloseButton();
    }

    @Override
    public void onClick(View view) {
        if (view.getId() == R.id.infobar_close_button) {
            mInfoBar.onCloseButtonClicked();
        } else {
            assert false;
        }
    }

    /**
     * Inserts a view before the close button.
     * @param view   View to insert.
     * @param weight Weight to assign to it.
     */
    // TODO(crbug/1056346): addContent is made public to allow access from InfoBar. Once
    // InfoBar is modularized, restore access to protected.
    public void addContent(View view, float weight) {
        LinearLayout.LayoutParams params;
        if (weight <= 0.0f) {
            params = new LinearLayout.LayoutParams(LayoutParams.WRAP_CONTENT, mCompactInfoBarSize);
        } else {
            params = new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, weight);
        }
        view.setMinimumHeight(mCompactInfoBarSize);
        params.gravity = Gravity.BOTTOM;
        addView(view, indexOfChild(mCloseButton), params);
    }

    /**
     * Adds an icon to the start of the infobar, if the infobar requires one.
     * @param iconResourceId Resource ID of the icon to use.
     * @param iconTintId The {@link ColorRes} used as tint for {@code iconResourceId}.
     * @param iconBitmap Raw {@link Bitmap} to use instead of a resource.
     */
    private void prepareIcon(int iconResourceId, @ColorRes int iconTintId, Bitmap iconBitmap) {
        ImageView iconView =
                InfoBarLayout.createIconView(getContext(), iconResourceId, iconTintId, iconBitmap);
        if (iconView != null) {
            LinearLayout.LayoutParams iconParams =
                    new LinearLayout.LayoutParams(mIconWidth, mCompactInfoBarSize);
            addView(iconView, iconParams);
        }
    }

    /**
     * Creates a close button that can be inserted into an infobar.
     * NOTE: This was forked from //chrome's InfoBarLayout.java, as WebLayer supports only compact
     * infobars and does not have a corresponding InfoBarLayout.java.
     * @param context Context to grab resources from.
     * @return {@link ImageButton} that represents a close button.
     */
    static ImageButton createCloseButton(Context context) {
        final ColorStateList tint =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color);
        TypedArray a =
                context.obtainStyledAttributes(new int[] {android.R.attr.selectableItemBackground});
        Drawable closeButtonBackground = a.getDrawable(0);
        a.recycle();

        ChromeImageButton closeButton = new ChromeImageButton(context);
        closeButton.setId(R.id.infobar_close_button);
        closeButton.setImageResource(R.drawable.btn_close);
        ApiCompatibilityUtils.setImageTintList(closeButton, tint);
        closeButton.setBackground(closeButtonBackground);
        closeButton.setContentDescription(context.getString(R.string.weblayer_infobar_close));
        closeButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

        return closeButton;
    }

    /** Adds a close button to the end of the infobar. */
    private View prepareCloseButton() {
        ImageButton closeButton = createCloseButton(getContext());
        closeButton.setOnClickListener(this);
        LinearLayout.LayoutParams closeParams =
                new LinearLayout.LayoutParams(mCompactInfoBarSize, mCompactInfoBarSize);
        addView(closeButton, closeParams);
        return closeButton;
    }

    /**
     * Helps building a standard message to display in a compact InfoBar. The message can feature
     * a link to perform and action from this infobar.
     */
    public static class MessageBuilder {
        private final InfoBarCompactLayout mLayout;
        private CharSequence mMessage;
        private CharSequence mLink;

        /** @param layout The layout we are building a message view for. */
        public MessageBuilder(InfoBarCompactLayout layout) {
            mLayout = layout;
        }

        public MessageBuilder withText(CharSequence message) {
            assert mMessage == null;
            mMessage = message;

            return this;
        }

        public MessageBuilder withText(@StringRes int messageResId) {
            assert mMessage == null;
            mMessage = mLayout.getResources().getString(messageResId);

            return this;
        }

        /** Appends a link after the main message, its displayed text being the specified string. */
        public MessageBuilder withLink(CharSequence label, Callback<View> onTapCallback) {
            assert mLink == null;

            final Resources resources = mLayout.getResources();
            SpannableString link = new SpannableString(label);
            link.setSpan(new NoUnderlineClickableSpan(resources, onTapCallback), 0, label.length(),
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            mLink = link;

            return this;
        }

        /**
         * Appends a link after the main message, its displayed text being constructed from the
         * given resource ID.
         */
        public MessageBuilder withLink(@StringRes int textResId, Callback<View> onTapCallback) {
            final Resources resources = mLayout.getResources();
            String label = resources.getString(textResId);
            return withLink(label, onTapCallback);
        }

        /** Finalizes the message view as set up in the builder and inserts it into the layout. */
        public void buildAndInsert() {
            mLayout.addContent(build(), 1f);
        }

        /**
         * Finalizes the message view as set up in the builder. The caller is responsible for adding
         * it to the parent layout.
         */
        public View build() {
            // TODO(dgn): Should be able to handle ReaderMode and Survey infobars but they have non
            // standard interaction models (no button/link, whole bar is a button) or style (large
            // rather than default text). Revisit after snowflake review.

            assert mMessage != null;

            final int messagePadding = mLayout.getResources().getDimensionPixelOffset(
                    R.dimen.infobar_compact_message_vertical_padding);

            SpannableStringBuilder builder = new SpannableStringBuilder();
            builder.append(mMessage);
            if (mLink != null) builder.append(" ").append(mLink);

            TextView prompt = new InfoBarMessageView(mLayout.getContext());
            ApiCompatibilityUtils.setTextAppearance(
                    prompt, R.style.TextAppearance_TextMedium_Primary);
            prompt.setText(builder);
            prompt.setGravity(Gravity.CENTER_VERTICAL);
            prompt.setPadding(0, messagePadding, 0, messagePadding);

            if (mLink != null) prompt.setMovementMethod(LinkMovementMethod.getInstance());

            return prompt;
        }
    }
}
