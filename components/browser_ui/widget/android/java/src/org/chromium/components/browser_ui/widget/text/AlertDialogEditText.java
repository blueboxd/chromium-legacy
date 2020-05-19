// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.text;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.EditText;
import androidx.appcompat.widget.AppCompatEditText;
import org.chromium.base.ApiCompatibilityUtils;

/**
 * Wrapper class needed due to b/122113958.
 *
 * Note that for password fields the hint text is expected to be set in XML so that it is available
 * during inflation. If the hint text or content description is changed programmatically, consider
 * calling {@link ApiCompatibilityUtils#setPasswordEditTextContentDescription(EditText)} after
 * the change.
 */
public class AlertDialogEditText extends AppCompatEditText {
    public AlertDialogEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        ApiCompatibilityUtils.setPasswordEditTextContentDescription(this);
    }
}
