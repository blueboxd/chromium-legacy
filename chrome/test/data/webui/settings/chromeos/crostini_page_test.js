// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

function setCrostiniPrefs(enabled, optional = {}) {
  const {
    sharedPaths = {},
    sharedUsbDevices = [],
    forwardedPorts = [],
    micSharing = false
  } = optional;
  crostiniPage.prefs = {
    crostini: {
      enabled: {value: enabled},
      port_forwarding: {ports: {value: forwardedPorts}},
      mic_sharing: {value: micSharing},
    },
    guest_os: {
      paths_shared_to_vms: {value: sharedPaths},
    },
  };
  crostiniBrowserProxy.sharedUsbDevices = sharedUsbDevices;
  Polymer.dom.flush();
}

/**
 * Checks whether a given element is visible to the user.
 * @param {!Element} element
 * @returns {boolean}
 */
function isVisible(element) {
  return !!(element && element.getBoundingClientRect().width > 0);
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      crostiniPage.async(resolve);
    });
  }

  suite('MainPage', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));
      assertFalse(button.disabled);

      button.click();
      Polymer.dom.flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', async function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);

      await flushAsync();
      assertFalse(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', true);

      await flushAsync();
      assertTrue(button.disabled);
      cr.webUIListenerCallback('crostini-installer-status-changed', false);

      await flushAsync();
      assertFalse(button.disabled);
    });
  });

  suite('SubPageDetails', function() {
    /** @type {?SettingsCrostiniSubPageElement} */
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExportImport: true,
        showCrostiniContainerUpgrade: true,
        showCrostiniPortForwarding: true,
        showCrostiniMic: true,
        showCrostiniDiskResize: true,
      });

      const eventPromise = new Promise((resolve) => {
                             const v = cr.addWebUIListener(
                                 'crostini-installer-status-changed', () => {
                                   resolve(v);
                                 });
                           }).then((v) => {
        assertTrue(cr.removeWebUIListener(v));
      });

      settings.Router.getInstance().navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();

      const pageLoadPromise = flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-subpage');
        assertTrue(!!subpage);
      });

      await Promise.all([pageLoadPromise, eventPromise]);
    });

    suite('SubPageDefault', function() {
      test('Sanity', function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        assertTrue(!!subpage.$$('#crostini-shared-usb-devices'));
        assertTrue(!!subpage.$$('#crostini-export-import'));
        assertTrue(!!subpage.$$('#remove'));
        assertTrue(!!subpage.$$('#container-upgrade'));
        assertTrue(!!subpage.$$('#crostini-port-forwarding'));
        assertTrue(!!subpage.$$('#crostini-mic-sharing'));
        assertTrue(!!subpage.$$('#crostini-disk-resize'));
      });

      test('SharedPaths', async function() {
        assertTrue(!!subpage.$$('#crostini-shared-paths'));
        subpage.$$('#crostini-shared-paths').click();

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-shared-paths');
        assertTrue(!!subpage);
      });

      test('ContainerUpgrade', function() {
        assertTrue(!!subpage.$$('#container-upgrade cr-button'));
        subpage.$$('#container-upgrade cr-button').click();
        assertEquals(
            1,
            crostiniBrowserProxy.getCallCount(
                'requestCrostiniContainerUpgradeView'));
      });

      test('ContainerUpgradeButtonDisabledOnUpgradeDialog', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
        assertTrue(!!button);

        await flushAsync();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', true);

        await flushAsync();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-upgrader-status-changed', false);

        await flushAsync();
        assertFalse(button.disabled);
      });

      test('ContainerUpgradeButtonDisabledOnInstall', async function() {
        const button = subpage.$$('#container-upgrade cr-button');
        assertTrue(!!button);

        await flushAsync();
        assertFalse(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await flushAsync();
        assertTrue(button.disabled);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await flushAsync();
        assertFalse(button.disabled);
      });

      test('Export', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(!!subpage.$$('#export cr-button'));
        subpage.$$('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });

      test('Import', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        subpage.$$('#import cr-button').click();

        await flushAsync();
        subpage = subpage.$$('settings-crostini-import-confirmation-dialog');
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
      });

      test('ExportImportButtonsGetDisabledOnOperationStatus', async function() {
        assertTrue(!!subpage.$$('#crostini-export-import'));
        subpage.$$('#crostini-export-import').click();

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', true);

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(subpage.$$('#export cr-button').disabled);
        assertTrue(subpage.$$('#import cr-button').disabled);
        cr.webUIListenerCallback(
            'crostini-export-import-operation-status-changed', false);

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertFalse(subpage.$$('#export cr-button').disabled);
        assertFalse(subpage.$$('#import cr-button').disabled);
      });

      test(
          'ExportImportButtonsDisabledOnWhenInstallingCrostini',
          async function() {
            assertTrue(!!subpage.$$('#crostini-export-import'));
            subpage.$$('#crostini-export-import').click();

            await flushAsync();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback('crostini-installer-status-changed', true);

            await flushAsync();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertTrue(subpage.$$('#export cr-button').disabled);
            assertTrue(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback(
                'crostini-installer-status-changed', false);

            await flushAsync();
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
          });

      test('ToggleCrostiniMicSharing', async function() {
        // Testing under the premise that Crostini is currently running and the
        // mic is being shared with Crostini.
        assertTrue(!!subpage.$$('#crostini-mic-sharing'));
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        setCrostiniPrefs(true, {micSharing: true});
        assertTrue(subpage.$$('#crostini-mic-sharing').checked);
        assertTrue(subpage.$$('#crostini-mic-sharing').pref.value);

        subpage.$$('#crostini-mic-sharing').click();
        await flushAsync();
        assertTrue(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        const dialog = subpage.$$('settings-crostini-mic-sharing-dialog');
        const dialogClosedPromise = test_util.eventToPromise('close', dialog);
        dialog.$$('cr-dialog cr-button').click();
        await Promise.all([dialogClosedPromise, flushAsync()]);
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
        assertFalse(subpage.$$('#crostini-mic-sharing').checked);
        assertFalse(subpage.$$('#crostini-mic-sharing').pref.value);

        subpage.$$('#crostini-mic-sharing').click();
        assertTrue(subpage.$$('#crostini-mic-sharing').checked);
        assertTrue(subpage.$$('#crostini-mic-sharing').pref.value);
        await flushAsync();
        // Dialog should only appear when a restart is required, as the setting
        // was initiated as true, changing the setting back to true does not
        // require a restart.
        assertFalse(!!subpage.$$('settings-crostini-mic-sharing-dialog'));
      });

      test('Remove', async function() {
        assertTrue(!!subpage.$$('#remove cr-button'));
        subpage.$$('#remove cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });

      test('RemoveHidden', async function() {
        // Elements are not destroyed when a dom-if stops being shown, but we
        // can check if their rendered width is non-zero. This should be
        // resilient against most formatting changes, since we're not relying on
        // them having any exact size, or on Polymer using any particular means
        // of hiding elements.
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', true);

        await flushAsync();
        assertFalse(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);

        await flushAsync();
        assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
      });

      test('HideOnDisable', async function() {
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI_DETAILS);
        setCrostiniPrefs(false);

        await test_util.eventToPromise('popstate', window);
        assertEquals(
            settings.Router.getInstance().getCurrentRoute(),
            settings.routes.CROSTINI);
      });

      test('DiskResizeOpensWhenClicked', async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', {succeeded: true, isUserChosenSize: true});
        subpage.$$('#showDiskResizeButton').click();

        await flushAsync();
        const dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertEquals(
            2, crostiniBrowserProxy.getCallCount('getCrostiniDiskInfo'));
      });
    });

    suite('SubPagePortForwarding', function() {
      /** @type {?SettingsCrostiniPortForwarding} */
      let subpage;
      setup(async function() {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
            {
              port_number: 5001,
              protocol_type: 1,
              label: 'Label2',
            },
          ]
        });

        await flushAsync();
        settings.Router.getInstance().navigateTo(
            settings.routes.CROSTINI_PORT_FORWARDING);

        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('Forwarded ports are shown', function() {
        // Extra list item for the titles.
        assertEquals(
            3, subpage.shadowRoot.querySelectorAll('.list-item').length);
      });

      test('AddPortSuccess', async function() {
        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushAsync();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        portNumberInput.value = '5000';
        const portLabelInput = subpage.$$('#portLabelInput');
        portLabelInput.value = 'Some Label';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
      });

      test('AddPortFail', async function() {
        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushAsync();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        const portNumberInput = subpage.$$('#portNumberInput');
        portNumberInput.value = 'INVALID_PORT_NUMBER';
        const portLabelInput = subpage.$$('#portLabelInput');
        portLabelInput.value = 'Some Label';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        portNumberInput.value = '65536';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            0, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
        portNumberInput.value = '65535';
        subpage.$$('cr-dialog cr-button[id="continue"]').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('addCrostiniPortForward'));
      });

      test('AddPortCancel', async function() {
        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        subpage.$$('#addPort cr-button').click();

        await flushAsync();
        subpage = subpage.$$('settings-crostini-add-port-dialog');
        subpage.$$('cr-dialog cr-button[id="cancel"]').click();


        await flushAsync();
        subpage = crostiniPage.$$('settings-crostini-port-forwarding');
        assertTrue(!!subpage);
      });

      test('RemoveAllPorts', function() {
        return flushAsync()
            .then(() => {
              subpage = crostiniPage.$$('settings-crostini-port-forwarding');
              subpage.$$('#showRemoveAllPortsMenu').click();
              return flushAsync();
            })
            .then(() => {
              subpage.$$('#removeAllPortsButton').click();
              assertEquals(
                  1,
                  crostiniBrowserProxy.getCallCount(
                      'removeAllCrostiniPortForwards'));
            });
      });

      test('RemoveSinglePort', function() {
        return flushAsync()
            .then(() => {
              subpage = crostiniPage.$$('settings-crostini-port-forwarding');
              subpage.$$('#showRemoveSinglePortMenu0').click();
              return flushAsync();
            })
            .then(() => {
              subpage.$$('#removeSinglePortButton').click();
              assertEquals(
                  1,
                  crostiniBrowserProxy.getCallCount(
                      'removeCrostiniPortForward'));
            });
      });


      test('ActivateSinglePort', function() {
        return flushAsync()
            .then(() => {
              subpage = crostiniPage.$$('settings-crostini-port-forwarding');
              subpage.$$('#toggleActivationButton0').click();
              return flushAsync();
            })
            .then(() => {
              assertEquals(
                  1,
                  crostiniBrowserProxy.getCallCount(
                      'activateCrostiniPortForward'));
            });
      });

      test('DeactivateSinglePort', function() {
        return flushAsync()
            .then(() => {
              subpage = crostiniPage.$$('settings-crostini-port-forwarding');
              const crToggle = subpage.$$('#toggleActivationButton0');
              crToggle.checked = true;
              crToggle.click();
              return flushAsync();
            })
            .then(() => {
              assertEquals(
                  1,
                  crostiniBrowserProxy.getCallCount(
                      'deactivateCrostiniPortForward'));
            });
      });

      test('ActivePortsChanged', async function() {
        setCrostiniPrefs(true, {
          forwardedPorts: [
            {
              port_number: 5000,
              protocol_type: 0,
              label: 'Label1',
            },
          ]
        });
        const crToggle = subpage.$$('#toggleActivationButton0');

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed',
            [{'port_number': 5000, 'protocol_type': 0}]);
        await flushAsync();
        assertTrue(crToggle.checked);

        cr.webUIListenerCallback(
            'crostini-port-forwarder-active-ports-changed', []);
        await flushAsync();
        assertFalse(crToggle.checked);
      });

      test('CrostiniStopAndStart', function() {
        const crToggle = subpage.$$('#toggleActivationButton0');
        assertFalse(crToggle.disabled);

        cr.webUIListenerCallback('crostini-status-changed', false);
        assertTrue(crToggle.disabled);

        cr.webUIListenerCallback('crostini-status-changed', true);
        assertFalse(crToggle.disabled);
      });
    });

    suite('DiskResize', async function() {
      let dialog;
      /**
       * Helper function to assert that the expected block is visible and the
       * others are not.
       * @param {!string} selector
       */
      function assertVisibleBlockIs(selector) {
        const selectors =
            ['#unsupported', '#resize-block', '#error', '#loading'];

        assertTrue(isVisible(dialog.$$(selector)));
        selectors.filter(s => s !== selector).forEach(s => {
          assertFalse(isVisible(dialog.$$(s)));
        });
      }

      const ticks = [
        {label: 'label 0', value: 0, ariaLabel: 'label 0'},
        {label: 'label 10', value: 10, ariaLabel: 'label 10'},
        {label: 'label 100', value: 100, ariaLabel: 'label 100'}
      ];

      const resizeableData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: true,
        defaultIndex: 2,
        ticks: ticks
      };

      const sparseDiskData = {
        succeeded: true,
        canResize: true,
        isUserChosenSize: false,
        defaultIndex: 2,
        ticks: ticks
      };

      async function clickShowDiskResize(userChosen) {
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo',
            {succeeded: true, isUserChosenSize: userChosen});
        subpage.$$('#showDiskResizeButton').click();
        await flushAsync();
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');

        if (userChosen) {
          // We should be on the loading page but unable to kick off a resize
          // yet.
          assertTrue(!!dialog.$$('#loading'));
          assertTrue(dialog.$$('#resize').disabled);
        }
      }

      setup(async function() {
        assertTrue(!!subpage.$$('#showDiskResizeButton'));
      });

      test('ResizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', resizeableData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskResizeShowButton'));
        assertEquals(subtext.innerText, 'label 100');
      });

      test('ReserveSizeButtonAndSubtextCorrectlySet', async function() {
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', sparseDiskData);
        const button = subpage.$$('#showDiskResizeButton');
        const subtext = subpage.$$('#diskSizeDescription');

        assertEquals(
            button.innerText,
            loadTimeData.getString('crostiniDiskReserveSizeButton'));
        assertEquals(
            subtext.innerText,
            loadTimeData.getString(
                'crostiniDiskResizeDynamicallyAllocatedSubtext'));
      });

      test('MessageShownIfErrorAndCanRetry', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', {succeeded: false, isUserChosenSize: true});

        // We failed, should have a retry button.
        let button = dialog.$$('#retry');
        assertVisibleBlockIs('#error');
        assertTrue(isVisible(button));
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // Back to the loading screen.
        button.click();
        await flushAsync();
        assertVisibleBlockIs('#loading');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        // And failure page again.
        await crostiniBrowserProxy.rejectPromise('getCrostiniDiskInfo');
        button = dialog.$$('#retry');
        assertTrue(isVisible(button));
        assertVisibleBlockIs('#error');
        assertTrue(dialog.$$('#resize').disabled);
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);

        assertEquals(
            3, crostiniBrowserProxy.getCallCount('getCrostiniDiskInfo'));
      });

      test('MessageShownIfCannotResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo',
            {succeeded: true, canResize: false, isUserChosenSize: true});
        assertVisibleBlockIs('#unsupported');
        assertTrue(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('ResizePageShownIfCanResize', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', resizeableData);
        assertVisibleBlockIs('#resize-block');

        assertEquals(ticks[0].label, dialog.$$('#label-begin').innerText);
        assertEquals(ticks[2].label, dialog.$$('#label-end').innerText);
        assertEquals(2, dialog.$$('#diskSlider').value);

        assertFalse(dialog.$$('#resize').disabled);
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('InProgressResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        await flushAsync();
        assertTrue(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertTrue(isVisible(dialog.$$('#resizing')));
        assertFalse(isVisible(dialog.$$('#resize-error')));
        assertTrue(dialog.$$('#cancel').disabled);
      });

      test('ErrorResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        await crostiniBrowserProxy.resolvePromise('resizeCrostiniDisk', false);
        assertFalse(button.disabled);
        assertFalse(isVisible(dialog.$$('#done')));
        assertFalse(isVisible(dialog.$$('#resizing')));
        assertTrue(isVisible(dialog.$$('#resize-error')));
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('SuccessResizing', async function() {
        await clickShowDiskResize(true);
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', resizeableData);
        const button = dialog.$$('#resize');
        button.click();
        await crostiniBrowserProxy.resolvePromise('resizeCrostiniDisk', true);
        assertFalse(button.disabled);
        assertTrue(isVisible(dialog.$$('#done')));
        assertFalse(isVisible(dialog.$$('#resizing')));
        assertFalse(isVisible(dialog.$$('#resize-error')));
        assertFalse(dialog.$$('#cancel').disabled);
      });

      test('DiskResizeConfirmationDialogShownAndAccepted', async function() {
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);
        // Dismiss confirmation.
        let confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#continue').click();
        await test_util.eventToPromise('close', confirmationDialog);
        assertFalse(isVisible(confirmationDialog));

        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));

        // Cancel main resize dialog.
        dialog.$$('#cancel').click();
        await test_util.eventToPromise('close', dialog);
        assertFalse(isVisible(dialog));

        // On another click, confirmation dialog should be shown again.
        await clickShowDiskResize(false);
        confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        confirmationDialog.$$('#continue').click();
        await test_util.eventToPromise('close', confirmationDialog);

        // Main dialog should show again.
        dialog = subpage.$$('settings-crostini-disk-resize-dialog');
        assertTrue(!!dialog);
        assertTrue(isVisible(dialog.$$('#resize')));
        assertTrue(isVisible(dialog.$$('#cancel')));
      });

      test('DiskResizeConfirmationDialogShownAndCanceled', async function() {
        await crostiniBrowserProxy.resolvePromise(
            'getCrostiniDiskInfo', sparseDiskData);
        await clickShowDiskResize(false);

        const confirmationDialog =
            subpage.$$('settings-crostini-disk-resize-confirmation-dialog');
        assertTrue(isVisible(confirmationDialog.$$('#continue')));
        assertTrue(isVisible(confirmationDialog.$$('#cancel')));
        confirmationDialog.$$('#cancel').click();
        await test_util.eventToPromise('close', confirmationDialog);

        assertFalse(!!subpage.$$('settings-crostini-disk-resize-dialog'));
      });
    });
  });

  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(
          true, {sharedPaths: {path1: ['termina'], path2: ['termina']}});

      await flushAsync();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_PATHS);

      await flushAsync();
      subpage = crostiniPage.$$('settings-crostini-shared-paths');
      assertTrue(!!subpage);
    });

    test('Sanity', function() {
      assertEquals(
          3, subpage.shadowRoot.querySelectorAll('.settings-box').length);
      assertEquals(2, subpage.shadowRoot.querySelectorAll('.list-item').length);
    });

    test('Remove', async function() {
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);
      assertFalse(subpage.$.crostiniList.hidden);
      assertTrue(subpage.$.crostiniListEmpty.hidden);
      assertTrue(!!subpage.$$('.list-item cr-icon-button'));

      {
        // Remove first shared path, still one left.
        subpage.$$('.list-item cr-icon-button').click();
        const [vmName, path] =
            await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
        assertEquals('termina', vmName);
        assertEquals('path1', path);
        setCrostiniPrefs(true, {sharedPaths: {path2: ['termina']}});
      }

      await flushAsync();
      Polymer.dom.flush();
      assertEquals(1, subpage.shadowRoot.querySelectorAll('.list-item').length);
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);

      {
        // Remove remaining shared path, none left.
        crostiniBrowserProxy.resetResolver('removeCrostiniSharedPath');
        subpage.$$('.list-item cr-icon-button').click();
        const [vmName, path] =
            await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
        assertEquals('termina', vmName);
        assertEquals('path2', path);
        setCrostiniPrefs(true, {sharedPaths: {}});
      }

      await flushAsync();
      Polymer.dom.flush();
      assertEquals(0, subpage.shadowRoot.querySelectorAll('.list-item').length);
      // Verify remove instructions are hidden, and empty list message
      // is shown.
      assertTrue(subpage.$.crostiniInstructionsRemove.hidden);
      assertTrue(subpage.$.crostiniList.hidden);
      assertFalse(subpage.$.crostiniListEmpty.hidden);
    });

    test('RemoveFailedRetry', async function() {
      // Remove shared path fails.
      crostiniBrowserProxy.removeSharedPathResult = false;
      subpage.$$('.list-item cr-icon-button').click();

      await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
      Polymer.dom.flush();
      assertTrue(subpage.$$('#removeSharedPathFailedDialog').open);

      // Click retry and make sure 'removeCrostiniSharedPath' is called
      // and dialog is closed/removed.
      crostiniBrowserProxy.removeSharedPathResult = true;
      subpage.$$('#removeSharedPathFailedDialog')
          .querySelector('.action-button')
          .click();
      await crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
      assertFalse(!!subpage.$$('#removeSharedPathFailedDialog'));
    });
  });

  suite('SubPageSharedUsbDevices', function() {
    let subpage;

    setup(async function() {
      setCrostiniPrefs(true, {
        sharedUsbDevices: [
          {shared: true, guid: '0001', name: 'usb_dev1'},
          {shared: false, guid: '0002', name: 'usb_dev2'},
          {shared: true, guid: '0003', name: 'usb_dev3'}
        ]
      });

      await flushAsync();
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_SHARED_USB_DEVICES);

      await flushAsync();
      subpage = crostiniPage.$$('settings-crostini-shared-usb-devices');
      assertTrue(!!subpage);
    });

    test('USB devices are shown', function() {
      assertEquals(3, subpage.shadowRoot.querySelectorAll('.toggle').length);
    });

    test('USB shared state is updated by toggling', async function() {
      assertTrue(!!subpage.$$('.toggle'));
      subpage.$$('.toggle').click();

      await flushAsync();
      Polymer.dom.flush();

      const args =
          await crostiniBrowserProxy.whenCalled('setCrostiniUsbDeviceShared');
      assertEquals('0001', args[0]);
      assertEquals(false, args[1]);

      // Simulate a change in the underlying model.
      cr.webUIListenerCallback('crostini-shared-usb-devices-changed', [
        {shared: true, guid: '0001', name: 'usb_dev1'},
      ]);
      Polymer.dom.flush();
      assertEquals(1, subpage.shadowRoot.querySelectorAll('.toggle').length);
    });
  });
});
