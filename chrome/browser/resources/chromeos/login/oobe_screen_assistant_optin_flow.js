// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe Assistant OptIn Flow screen implementation.
 */

login.createScreen(
    'AssistantOptInFlowScreen', 'assistant-optin-flow', function() {
      return {
        EXTERNAL_API: [
          'reloadContent', 'addSettingZippy', 'showNextScreen',
          'onVoiceMatchUpdate'
        ],

        /** Initial UI State for screen */
        getOobeUIInitialState() {
          return OOBE_UI_STATE.ONBOARDING;
        },

        /** @Override */
        onBeforeShow(data) {
          $('assistant-optin-flow-card').onShow();
        },

        /**
         * Reloads localized strings.
         * @param {!Object} data New dictionary with i18n values.
         */
        reloadContent(data) {
          $('assistant-optin-flow-card').reloadContent(data);
        },

        /**
         * Add a setting zippy object in the corresponding screen.
         * @param {string} type type of the setting zippy.
         * @param {!Object} data String and url for the setting zippy.
         */
        addSettingZippy(type, data) {
          $('assistant-optin-flow-card').addSettingZippy(type, data);
        },

        /**
         * Show the next screen in the flow.
         */
        showNextScreen() {
          $('assistant-optin-flow-card').showNextScreen();
        },

        /**
         * Called when the Voice match state is updated.
         * @param {string} state the voice match state.
         */
        onVoiceMatchUpdate(state) {
          $('assistant-optin-flow-card').onVoiceMatchUpdate(state);
        },
      };
    });
