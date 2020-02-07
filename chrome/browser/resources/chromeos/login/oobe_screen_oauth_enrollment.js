// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('OAuthEnrollmentScreen', 'oauth-enrollment', function() {
  return {
    EXTERNAL_API: [
      'showStep', 'showError', 'doReload', 'showAttributePromptStep',
      'setAdJoinParams', 'setAdJoinConfiguration',
      'setEnterpriseDomainAndDeviceType'
    ],

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('enterprise-enrollment');
    },

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.ENROLLMENT;
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent() {
      $('enterprise-enrollment').updateLocalizedContent();
    },


    /** @override */
    decorate() {
      $('enterprise-enrollment').screen = this;
    },

    /**
     * Shows attribute-prompt step with pre-filled asset ID and
     * location.
     */
    showAttributePromptStep(annotatedAssetId, annotatedLocation) {
      $('enterprise-enrollment')
          .showAttributePromptStep(annotatedAssetId, annotatedLocation);
    },

    /**
     * Sets the type of the device and the enterprise domain to be shown.
     *
     * @param {string} enterprise_domain
     * @param {string} device_type
     */
    setEnterpriseDomainAndDeviceType(enterprise_domain, device_type) {
      $('enterprise-enrollment')
          .setEnterpriseDomainAndDeviceType(enterprise_domain, device_type);
    },

    /**
     * Switches between the different steps in the enrollment flow.
     * @param {string} step the steps to show, one of "signin", "working",
     * "attribute-prompt", "error", "success".
     */
    showStep(step) {
      $('enterprise-enrollment').showStep(step);
    },

    /**
     * Sets an error message and switches to the error screen.
     * @param {string} message the error message.
     * @param {boolean} retry whether the retry link should be shown.
     */
    showError(message, retry) {
      $('enterprise-enrollment').showError(message, retry);
    },

    doReload() {
      $('enterprise-enrollment').doReload();
    },

    /**
     * Sets Active Directory join screen params.
     * @param {string} machineName
     * @param {string} userName
     * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
     * @param {boolean} showUnlockConfig true if there is an encrypted
     * configuration (and not unlocked yet).
     */
    setAdJoinParams(machineName, userName, errorState, showUnlockConfig) {
      $('enterprise-enrollment')
          .setAdJoinParams(machineName, userName, errorState, showUnlockConfig);
    },

    /**
     * Sets Active Directory join screen with the unlocked configuration.
     * @param {Array<JoinConfigType>} options
     */
    setAdJoinConfiguration(options) {
      $('enterprise-enrollment').setAdJoinConfiguration(options);
    },

    closeEnrollment_(result) {
      chrome.send('oauthEnrollClose', [result]);
    },

    onAttributesEntered_(asset_id, location) {
      chrome.send('oauthEnrollAttributes', [asset_id, location]);
    },

    onAuthCompleted_(email) {
      chrome.send('oauthEnrollCompleteLogin', [email]);
    },

    onAuthFrameLoaded_() {
      chrome.send('frameLoadingCompleted');
    },

    onAdCompleteLogin_(
        machine_name, distinguished_name, encryption_types, username,
        password) {
      chrome.send('oauthEnrollAdCompleteLogin', [
        machine_name, distinguished_name, encryption_types, username, password
      ]);
    },

    onAdUnlockConfiguration_(unlock_password) {
      chrome.send('oauthEnrollAdUnlockConfiguration', [unlock_password]);
    },
  };
}, {
  changeRequisitonProhibited: true,
});
