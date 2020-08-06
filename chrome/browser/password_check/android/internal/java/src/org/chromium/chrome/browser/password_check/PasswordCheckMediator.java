// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;

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
        assert items.size() >= 1 : "Needs to initialize list with header before adding items!";

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
        if (items.size() == 0) {
            items.add(new ListItem(PasswordCheckProperties.ItemType.HEADER,
                    new PropertyModel.Builder(PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                            .with(CHECK_STATUS, status)
                            .build()));
        } else {
            items.get(0).model.set(CHECK_STATUS, status);
        }
    }

    @Override
    public void onCompromisedCredentialFound(
            String originUrl, String username, String password, boolean hasScript) {
        assert originUrl != null;
        assert username != null;
        assert password != null;
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() >= 1 : "Needs to initialize list with header before adding items!";

        CompromisedCredential credential =
                new CompromisedCredential(originUrl, username, password, false, hasScript);
        items.add(new ListItem(credential.hasScript()
                        ? PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL_WITH_SCRIPT
                        : PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                new PropertyModel
                        .Builder(PasswordCheckProperties.CompromisedCredentialProperties.ALL_KEYS)
                        .with(COMPROMISED_CREDENTIAL, credential)
                        .with(CREDENTIAL_HANDLER, this)
                        .build()));
    }

    @Override
    public void onRemove(CompromisedCredential credential) {
        mDelegate.removeCredential(credential);
    }

    @Override
    public void onChangePasswordButtonClick(CompromisedCredential credential) {
        mLaunchCctWithChangePasswordUrl.accept(credential.getOriginUrl());
    }

    @Override
    public void onChangePasswordWithScriptButtonClick(CompromisedCredential credential) {
        assert credential.hasScript();
        mLaunchCctWithScript.accept(credential);
    }

    private PasswordCheck getPasswordCheck() {
        PasswordCheck passwordCheck = PasswordCheckFactory.getOrCreate();
        assert passwordCheck != null : "Password Check UI component needs native counterpart!";
        return passwordCheck;
    }
}
