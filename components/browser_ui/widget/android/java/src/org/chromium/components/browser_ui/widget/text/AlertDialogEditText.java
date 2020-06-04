// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.text;

import android.annotation.SuppressLint;
import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ViewStructure;
import android.widget.EditText;

import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.VerifiesOnO;

/**
 * Wrapper class needed due to b/122113958.
 *
 * Note that for password fields the hint text is expected to be set in XML so that it is available
 * during inflation. If the hint text or content description is changed programmatically, consider
 * calling {@link ApiCompatibilityUtils#setPasswordEditTextContentDescription(EditText)} after
 * the change.
 */
@VerifiesOnO
public class AlertDialogEditText extends AppCompatEditText {
    private String mUrl;

    public AlertDialogEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setUrl(String url) {
        mUrl = url;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        ApiCompatibilityUtils.setPasswordEditTextContentDescription(this);
    }

    @Override
    @SuppressLint("NewApi")
    public void onProvideAutofillStructure(ViewStructure structure, int flags) {
        if (!TextUtils.isEmpty(mUrl)) {
            structure.setWebDomain(mUrl);
        }
        super.onProvideAutofillStructure(structure, flags);
    }
}
