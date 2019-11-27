// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.Html;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;

import java.util.ArrayList;
import java.util.IllegalFormatException;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Helper class to format templated strings when rendering cards. */
public class ParameterizedTextEvaluator {
    private static final String TAG = "ParameterizedTextEvalua";
    private final Clock mClock;

    ParameterizedTextEvaluator(Clock clock) {
        this.mClock = clock;
    }

    /** Evaluates the given parameterized string, respecting the HTML setting */
    public CharSequence evaluate(AssetProvider assetProvider, ParameterizedText templatedString) {
        if (!templatedString.hasText()) {
            Logger.w(TAG, "Got templated string with no display string");
            return templatedString.getText();
        }

        String evaluated = evaluateText(assetProvider, templatedString);

        if (templatedString.getIsHtml()) {
            if (Build.VERSION.SDK_INT < VERSION_CODES.N) {
                return Html.fromHtml(evaluated);
            } else {
                return Html.fromHtml(evaluated, Html.FROM_HTML_MODE_LEGACY);
            }
        } else {
            return evaluated;
        }
    }

    /**
     * Evaluates the given ParameterizedText, and returns the raw evaluated value, without any Html
     * wrapping.
     */
    private String evaluateText(AssetProvider assetProvider, ParameterizedText templatedString) {
        if (!templatedString.hasText()) {
            Logger.w(TAG, "Got templated string with no display string");
            return templatedString.getText();
        }

        if (templatedString.getParametersCount() == 0) {
            return templatedString.getText();
        }

        List<String> params = new ArrayList<>();
        for (ParameterizedText.Parameter param : templatedString.getParametersList()) {
            String value = evaluateParam(assetProvider, param);
            // Add a placeholder for any invalid parameters so that it will be obvious which one
            // failed.
            if (value == null) {
                value = "(invalid param)";
            }
            params.add(value);
        }

        String displayString = templatedString.getText();
        try {
            return String.format(
                    displayString, (Object[]) params.toArray(new String[params.size()]));
        } catch (IllegalFormatException e) {
            // Don't crash if we get invalid data - just log the display string and the error.
            Logger.e(TAG, e, "Error formatting display string \"%s\"", displayString);
            return displayString;
        }
    }

    /*@Nullable*/
    private String evaluateParam(AssetProvider assetProvider, ParameterizedText.Parameter param) {
        if (param.hasTimestampSeconds()) {
            long elapsedTimeMillis = mClock.currentTimeMillis()
                    - TimeUnit.SECONDS.toMillis(param.getTimestampSeconds());
            return assetProvider.getRelativeElapsedString(elapsedTimeMillis);
        }
        return null;
    }
}
