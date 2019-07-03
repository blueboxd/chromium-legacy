// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Handler;
import android.support.v4.view.ViewCompat;
import android.view.MotionEvent;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding EntitySuggestion properties to its view. */
public class EntitySuggestionViewBinder {
    /** IMAGE_DOMINANT_COLOR Value indicating no fallback color for suggestion. */
    static final int NO_DOMINANT_COLOR = Color.TRANSPARENT;

    private static final class EventListener implements EntitySuggestionView.EventListener {
        final Handler mHandler;
        final SuggestionViewDelegate mDelegate;

        EventListener(EntitySuggestionView view, SuggestionViewDelegate delegate) {
            mHandler = new Handler();
            mDelegate = delegate;
        }

        @Override
        public void onMotionEvent(MotionEvent ev) {
            // Whenever the suggestion dropdown is touched, we dispatch onGestureDown which is
            // used to let autocomplete controller know that it should stop updating suggestions.
            switch (ev.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    mDelegate.onGestureDown();
                    break;
                case MotionEvent.ACTION_UP:
                    mDelegate.onGestureUp(ev.getEventTime());
                    break;
            }
        }

        @Override
        public void onClick() {
            postAction(() -> mDelegate.onSelection());
        }

        @Override
        public void onLongClick() {
            postAction(() -> mDelegate.onLongPress());
        }

        @Override
        public void onSelected() {
            postAction(() -> mDelegate.onSetUrlToSuggestion());
        }

        @Override
        public void onRefine() {
            postAction(() -> mDelegate.onRefineSuggestion());
        }

        /**
         * Post delegate action to main thread.
         * @param action Delegate action to invoke on the UI thread.
         */
        private void postAction(Runnable action) {
            if (!mHandler.post(action)) action.run();
        }
    }

    private static void applySuggestionImage(PropertyModel model, EntitySuggestionView view) {
        Bitmap bitmap = model.get(EntitySuggestionViewProperties.IMAGE_BITMAP);
        int color = model.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR);

        if (bitmap != null) {
            view.setSuggestionImage(bitmap);
        } else if (color != NO_DOMINANT_COLOR) {
            // Note: we deliberately don't use ColorDrawable here to allow presenting
            // fallback color as a rounded-corners rectangle.
            // See EntitySuggestionView#setSuggestionImage for more details.
            bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
            bitmap.eraseColor(color);
            view.setSuggestionImage(bitmap);
        } else {
            view.clearSuggestionImage();
        }
    }

    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(
            PropertyModel model, EntitySuggestionView view, PropertyKey propertyKey) {
        if (EntitySuggestionViewProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(
                    new EventListener(view, model.get(EntitySuggestionViewProperties.DELEGATE)));
        } else if (EntitySuggestionViewProperties.SUBJECT_TEXT.equals(propertyKey)) {
            view.setSubjectText(model.get(EntitySuggestionViewProperties.SUBJECT_TEXT));
        } else if (EntitySuggestionViewProperties.DESCRIPTION_TEXT.equals(propertyKey)) {
            view.setDescriptionText(model.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT));
        } else if (EntitySuggestionViewProperties.IMAGE_BITMAP.equals(propertyKey)
                || EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR.equals(propertyKey)) {
            applySuggestionImage(model, view);
        } else if (SuggestionCommonProperties.USE_DARK_COLORS.equals(propertyKey)) {
            view.setUseDarkColors(model.get(SuggestionCommonProperties.USE_DARK_COLORS));
        } else if (SuggestionCommonProperties.LAYOUT_DIRECTION.equals(propertyKey)) {
            ViewCompat.setLayoutDirection(
                    view, model.get(SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }
}
