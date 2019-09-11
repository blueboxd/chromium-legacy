// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.rename;

import static android.content.Context.INPUT_METHOD_SERVICE;

import android.content.Context;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ui.widget.text.AlertDialogEditText;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.RenameResult;

/**
 * Content View of dialog in Download Home that allows users to rename a downloaded file.
 */
public class RenameDialogCustomView extends ScrollView {
    private TextView mSubtitleView;
    private AlertDialogEditText mFileName;
    private Callback</*Empty*/ Boolean> mEmptyFileNameObserver;

    public RenameDialogCustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // ScrollView implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSubtitleView = findViewById(R.id.subtitle);
        mFileName = findViewById(R.id.file_name);
        mFileName.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (mEmptyFileNameObserver == null) return;
                mEmptyFileNameObserver.onResult(getTargetName().isEmpty());
            }
        });
    }

    /**
     * @param callback Callback to run when edit text is empty.
     */
    public void setEmptyInputObserver(Callback<Boolean> callback) {
        mEmptyFileNameObserver = callback;
    }

    /**
     * @param renameResult RenameResult to distinguish dialog type, used for updating the subtitle
     *         view.
     * @param suggestedName The content to update the display on EditText box.
     */
    public void updateToErrorView(String suggestedName, @RenameResult int renameResult) {
        if (renameResult == RenameResult.SUCCESS) return;
        if (!TextUtils.isEmpty(suggestedName)) {
            mFileName.setText(suggestedName);
        }
        mFileName.clearFocus();
        highlightEditText(suggestedName);
        mSubtitleView.setVisibility(View.VISIBLE);
        switch (renameResult) {
            case RenameResult.FAILURE_NAME_CONFLICT:
                mSubtitleView.setText(R.string.rename_failure_name_conflict);
                break;
            case RenameResult.FAILURE_NAME_TOO_LONG:
                mSubtitleView.setText(R.string.rename_failure_name_too_long);
                break;
            case RenameResult.FAILURE_NAME_INVALID:
                mSubtitleView.setText(R.string.rename_failure_name_invalid);
                break;
            case RenameResult.FAILURE_UNAVAILABLE:
                mSubtitleView.setText(R.string.rename_failure_unavailable);
                break;
            default:
                break;
        }
    }

    /**
     * @param suggestedName String value display in the EditTextBox.
     * Initialize components in view: hide subtitle and reset value in the editTextBox.
     */
    public void initializeView(String suggestedName) {
        mSubtitleView.setVisibility(View.GONE);
        if (!TextUtils.isEmpty(suggestedName)) {
            mFileName.setText(suggestedName);
        }
        mFileName.clearFocus();
        highlightEditText(suggestedName);
    }

    /**
     * @return A String from user input for the target name.
     */
    public String getTargetName() {
        return mFileName.getText().toString();
    }

    private void highlightEditText(String name) {
        highlightEditText(0, name.length() - RenameUtils.getFileExtension(name).length());
    }

    /**
     * @param startIndex Start index of selection.
     * @param endIndex End index of selection.
     * Select renamable title portion, require focus and acquire the soft keyboard.
     */
    private void highlightEditText(int startIndex, int endIndex) {
        mFileName.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (!hasFocus) return;

                if (endIndex <= 0 || startIndex > endIndex
                        || endIndex >= getTargetName().length() - 1) {
                    mFileName.selectAll();
                } else {
                    mFileName.setSelection(startIndex, endIndex);
                }
            }
        });

        post(this::showSoftInput);
    }

    private void showSoftInput() {
        if (mFileName.requestFocus()) {
            InputMethodManager inputMethodManager =
                    (InputMethodManager) mFileName.getContext().getSystemService(
                            INPUT_METHOD_SERVICE);
            inputMethodManager.showSoftInput(mFileName, InputMethodManager.SHOW_FORCED);
        }
    }
}