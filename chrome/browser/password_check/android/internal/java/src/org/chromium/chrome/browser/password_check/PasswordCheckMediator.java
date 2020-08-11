// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_CONFIRMATION_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_ORIGIN;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.RESTART_BUTTON_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;

import android.content.DialogInterface;
import android.util.Pair;

import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Consumer;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the PasswordCheck component. It sets the state of the model and reacts to
 * events like clicks.
 */
class PasswordCheckMediator
        implements PasswordCheckCoordinator.CredentialEventHandler, PasswordCheck.Observer {
    private PropertyModel mModel;
    private PasswordCheckComponentUi.Delegate mDelegate;
    private final Consumer<String> mLaunchCctWithChangePasswordUrl;
    private final Consumer<CompromisedCredential> mLaunchCctWithScript;

    PasswordCheckMediator(Consumer<String> launchCctWithChangePasswordUrl,
            Consumer<CompromisedCredential> launchCctWithScript) {
        this.mLaunchCctWithChangePasswordUrl = launchCctWithChangePasswordUrl;
        this.mLaunchCctWithScript = launchCctWithScript;
    }

    void initialize(PropertyModel model, PasswordCheckComponentUi.Delegate delegate) {
        mModel = model;
        mDelegate = delegate;
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() == 0;

        items.add(new ListItem(PasswordCheckProperties.ItemType.HEADER,
                new PropertyModel.Builder(PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                        .with(CHECK_PROGRESS, UNKNOWN_PROGRESS)
                        .with(CHECK_STATUS, PasswordCheckUIStatus.RUNNING)
                        .with(CHECK_TIMESTAMP, null)
                        .with(COMPROMISED_CREDENTIALS_COUNT, null)
                        .with(RESTART_BUTTON_ACTION, this::runCheck)
                        .build()));
        getPasswordCheck().addObserver(this, true);
    }

    void destroy() {
        getPasswordCheck().removeObserver(this);
    }

    @Override
    public void onCompromisedCredentialsFetchCompleted() {
        CompromisedCredential[] credentials = getPasswordCheck().getCompromisedCredentials();
        assert credentials != null;
        ListModel<ListItem> items = mModel.get(ITEMS);
        if (items.size() == 0) {
            items.add(new ListItem(PasswordCheckProperties.ItemType.HEADER,
                    new PropertyModel.Builder(PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                            .with(CHECK_STATUS, PasswordCheckUIStatus.RUNNING)
                            .with(RESTART_BUTTON_ACTION, this::runCheck)
                            .build()));
        }
        if (items.size() > 1) items.removeRange(1, items.size() - 1);

        for (CompromisedCredential credential : credentials) {
            items.add(new ListItem(credential.hasScript()
                            ? PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL_WITH_SCRIPT
                            : PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                    new PropertyModel
                            .Builder(PasswordCheckProperties.CompromisedCredentialProperties
                                             .ALL_KEYS)
                            .with(COMPROMISED_CREDENTIAL, credential)
                            .with(CREDENTIAL_HANDLER, this)
                            .build()));
        }
    }

    @Override
    public void onSavedPasswordsFetchCompleted() {}

    @Override
    public void onPasswordCheckStatusChanged(@PasswordCheckUIStatus int status) {
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1;

        PropertyModel header = items.get(0).model;
        header.set(CHECK_STATUS, status);
        header.set(
                CHECK_PROGRESS, status == PasswordCheckUIStatus.RUNNING ? UNKNOWN_PROGRESS : null);
        Long checkTimestamp = null;
        Integer compromisedCredentialCount = null;
        if (status == PasswordCheckUIStatus.IDLE) {
            compromisedCredentialCount = getPasswordCheck().getCompromisedCredentialsCount();
            checkTimestamp = getPasswordCheck().getCheckTimestamp();
        }
        header.set(CHECK_TIMESTAMP, checkTimestamp);
        header.set(COMPROMISED_CREDENTIALS_COUNT, compromisedCredentialCount);
    }

    void onPasswordCheckProgressChanged(Pair<Integer, Integer> progress) {
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1;
        assert progress.first >= 0;
        assert progress.second >= progress.first;

        PropertyModel header = items.get(0).model;
        header.set(CHECK_STATUS, PasswordCheckUIStatus.RUNNING);
        header.set(CHECK_PROGRESS, progress);
        header.set(CHECK_TIMESTAMP, null);
        header.set(COMPROMISED_CREDENTIALS_COUNT, null);
    }

    @Override
    public void onCompromisedCredentialFound(CompromisedCredential leakedCredential) {
        assert leakedCredential != null;
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1 : "Needs to initialize list with header before adding items!";
        items.add(new ListItem(leakedCredential.hasScript()
                        ? PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL_WITH_SCRIPT
                        : PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                new PropertyModel
                        .Builder(PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, leakedCredential)
                        .with(CREDENTIAL_HANDLER, this)
                        .build()));
    }

    @Override
    public void onEdit(CompromisedCredential credential) {
        // TODO(crbug.com/1114720): Implement.
    }

    @Override
    public void onRemove(CompromisedCredential credential) {
        mModel.set(DELETION_ORIGIN, credential.getDisplayOrigin());
        mModel.set(
                DELETION_CONFIRMATION_HANDLER, new PasswordCheckDeletionDialogFragment.Handler() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        if (which != AlertDialog.BUTTON_POSITIVE) return;
                        mDelegate.removeCredential(credential);
                        mModel.set(DELETION_CONFIRMATION_HANDLER, null);
                        mModel.set(DELETION_ORIGIN, null);
                    }

                    @Override
                    public void onDismiss() {
                        mModel.set(DELETION_CONFIRMATION_HANDLER, null);
                    }
                });
    }

    @Override
    public void onChangePasswordButtonClick(CompromisedCredential credential) {
        mLaunchCctWithChangePasswordUrl.accept(credential.getSignonRealm());
    }

    @Override
    public void onChangePasswordWithScriptButtonClick(CompromisedCredential credential) {
        assert credential.hasScript();
        mLaunchCctWithScript.accept(credential);
    }

    private void runCheck() {
        getPasswordCheck().startCheck();
    }

    private PasswordCheck getPasswordCheck() {
        PasswordCheck passwordCheck = PasswordCheckFactory.getOrCreate();
        assert passwordCheck != null : "Password Check UI component needs native counterpart!";
        return passwordCheck;
    }
}
