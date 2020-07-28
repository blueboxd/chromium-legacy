// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.OnLifecycleEvent;

import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates the PasswordCheckComponentUi. This class is responsible for managing the UI for the check
 * of the leaked password.
 */
class PasswordCheckCoordinator implements PasswordCheckComponentUi, LifecycleObserver {
    private final PasswordCheckFragmentView mFragmentView;
    private PropertyModel mModel;

    /**
     * Blueprint for a class that handles interactions with credentials.
     */
    interface CredentialEventHandler {
        /**
         * Removes the given Credential from the password store.
         * @param credential A {@link CompromisedCredential} to be removed.
         */
        void onRemove(CompromisedCredential credential);
    }

    PasswordCheckCoordinator(PasswordCheckFragmentView fragmentView) {
        mFragmentView = fragmentView;
        // TODO(crbug.com/1101256): If help is part of the view, make mediator the delegate.
        mFragmentView.setComponentDelegate(this);
        mFragmentView.getLifecycle().addObserver(this);
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    public void connectToModelWhenViewIsReady() {
        // In the rare case of a restarted activity, don't recreate the model and mediator.
        if (mModel == null) {
            mModel = PasswordCheckProperties.createDefaultModel();
            PasswordCheckMediator mediator = new PasswordCheckMediator();
            PasswordCheckCoordinator.setUpModelChangeProcessors(mModel, mFragmentView);
            mediator.initialize(mModel, PasswordCheckFactory.getOrCreate());
        }
    }

    // TODO(crbug.com/1101256): Move to view code.
    @Override
    public boolean handleHelp(MenuItem item) {
        if (item.getItemId() == org.chromium.chrome.R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(mFragmentView.getActivity(),
                    mFragmentView.getActivity().getString(
                            org.chromium.chrome.R.string.help_context_check_passwords),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void destroy() {
        PasswordCheckFactory.destroy();
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link PasswordCheckProperties}.
     * @param view A {@link PasswordCheckFragmentView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, PasswordCheckFragmentView view) {
        PropertyModelChangeProcessor.create(
                model, view, PasswordCheckViewBinder::bindPasswordCheckView);
    }
}