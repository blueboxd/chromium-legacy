// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen(
    'EncryptionMigrationScreen', 'encryption-migration', function() {
      return {
        EXTERNAL_API: [
          'setUIState',
          'setMigrationProgress',
          'setIsResuming',
          'setBatteryState',
          'setNecessaryBatteryPercent',
          'setAvailableSpaceInString',
          'setNecessarySpaceInString',
        ],

        /**
         * Ignore any accelerators the user presses on this screen.
         */
        ignoreAccelerators: true,

        /**
         * Returns default event target element.
         */
        get defaultControl() {
          return $('encryption-migration-element');
        },

        /** Initial UI State for screen */
        getOobeUIInitialState() {
          return OOBE_UI_STATE.MIGRATION;
        },

        /*
         * Executed on language change.
         */
        updateLocalizedContent() {
          $('encryption-migration-element').i18nUpdateLocale();
        },

        /** @override */
        decorate() {
          var encryptionMigration = $('encryption-migration-element');
          encryptionMigration.addEventListener('upgrade', function() {
            chrome.send('startMigration');
          });
          encryptionMigration.addEventListener('skip', function() {
            chrome.send('skipMigration');
          });
          encryptionMigration.addEventListener(
              'restart-on-low-storage', function() {
                chrome.send('requestRestartOnLowStorage');
              });
          encryptionMigration.addEventListener(
              'restart-on-failure', function() {
                chrome.send('requestRestartOnFailure');
              });
          encryptionMigration.addEventListener(
              'openFeedbackDialog', function() {
                chrome.send('openFeedbackDialog');
              });
        },

        /**
         * Updates the migration screen by specifying a state which corresponds
         * to a sub step in the migration process.
         * @param {EncryptionMigrationUIState} state The UI state to identify a
         *     sub step in migration.
         */
        setUIState(state) {
          $('encryption-migration-element').uiState = state;
        },

        /**
         * Updates the migration progress.
         * @param {number} progress The progress of migration in range [0, 1].
         */
        setMigrationProgress(progress) {
          $('encryption-migration-element').progress = progress;
        },

        /**
         * Updates the migration screen based on whether the migration process
         * is resuming the previous one.
         * @param {boolean} isResuming
         */
        setIsResuming(isResuming) {
          $('encryption-migration-element').isResuming = isResuming;
        },

        /**
         * Updates battery level of the device.
         * @param {number} batteryPercent Battery level in percent.
         * @param {boolean} isEnoughBattery True if the battery is enough.
         * @param {boolena} isCharging True if the device is connected to power.
         */
        setBatteryState(batteryPercent, isEnoughBattery, isCharging) {
          var element = $('encryption-migration-element');
          element.batteryPercent = Math.floor(batteryPercent);
          element.isEnoughBattery = isEnoughBattery;
          element.isCharging = isCharging;
        },

        /**
         * Update the necessary battery percent to start migration in the UI.
         * @param {number} necessaryBatteryPercent Necessary battery level.
         */
        setNecessaryBatteryPercent(necessaryBatteryPercent) {
          $('encryption-migration-element').necessaryBatteryPercent =
              necessaryBatteryPercent;
        },

        /**
         * Updates the string representation of available space size.
         * @param {string} space
         */
        setAvailableSpaceInString(space) {
          $('encryption-migration-element').availableSpaceInString = space;
        },

        /**
         * Updates the string representation of necessary space size.
         * @param {string} space
         */
        setNecessarySpaceInString(space) {
          $('encryption-migration-element').necessarySpaceInString = space;
        },
      };
    });
