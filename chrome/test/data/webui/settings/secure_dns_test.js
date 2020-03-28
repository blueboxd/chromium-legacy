// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input.
 */

// clang-format off
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.m.js';
// #import {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

suite('SettingsSecureDnsInput', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SecureDnsInputElement} */
  let testElement;

  /** @type {CrInputElement} */
  let crInput;

  // Possible error messages
  const invalidFormat = 'invalid format description';
  const probeFail = 'probe fail description';

  const invalidEntry = 'invalid_entry';
  const validFailEntry = 'https://example.server/dns-query';
  const validSuccessEntry = 'https://example.server.another/dns-query';

  suiteSetup(function() {
    loadTimeData.overrideValues({
      secureDnsCustomFormatError: invalidFormat,
      secureDnsCustomConnectionError: probeFail,
    });
  });

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('secure-dns-input');
    document.body.appendChild(testElement);
    Polymer.dom.flush();
    crInput = testElement.$$('#input');
    assertFalse(crInput.invalid);
    assertEquals('', testElement.value);
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsInputEmpty', async function() {
    // Trigger validation on an empty input.
    testBrowserProxy.setValidEntry('');
    testElement.validate();
    assertEquals(
        '', await testBrowserProxy.whenCalled('validateCustomDnsEntry'));
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputValidFormatAndProbeFail', async function() {
    // Enter two valid servers but make the first one fail the test query.
    testElement.value = `${validFailEntry} ${validSuccessEntry}`;
    testBrowserProxy.setValidEntry(validFailEntry);
    testBrowserProxy.setProbeSuccess(false);
    testElement.validate();
    assertEquals(
        `${validFailEntry} ${validSuccessEntry}`,
        await testBrowserProxy.whenCalled('validateCustomDnsEntry'));
    assertEquals(
        validFailEntry,
        await testBrowserProxy.whenCalled('probeCustomDnsTemplate'));
    assertTrue(crInput.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(probeFail, crInput.errorMessage);
  });

  test('SecureDnsInputValidFormatAndProbeSuccess', async function() {
    // Enter a valid input and make the test query succeed.
    testElement.value = validSuccessEntry;
    testBrowserProxy.setValidEntry(validSuccessEntry);
    testBrowserProxy.setProbeSuccess(true);
    testElement.validate();
    assertEquals(
        validSuccessEntry,
        await testBrowserProxy.whenCalled('validateCustomDnsEntry'));
    assertEquals(
        validSuccessEntry,
        await testBrowserProxy.whenCalled('probeCustomDnsTemplate'));
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputInvalid', async function() {
    // Enter an invalid input and trigger validation.
    testElement.value = invalidEntry;
    testBrowserProxy.setValidEntry('');
    testElement.validate();
    assertEquals(
        invalidEntry,
        await testBrowserProxy.whenCalled('validateCustomDnsEntry'));
    assertTrue(crInput.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(invalidFormat, crInput.errorMessage);

    // Trigger an input event and check that the error clears.
    crInput.fire('input');
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
    assertEquals(invalidEntry, testElement.value);
  });
});

suite('SettingsSecureDns', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsSecureDnsElement} */
  let testElement;

  /** @type {SettingsToggleButtonElement} */
  let secureDnsToggle;

  /** @type {CrRadioGroupElement} */
  let secureDnsRadioGroup;

  /** @type {!Array<!settings.ResolverOption>} */
  const resolverList = [
    {name: 'Custom', value: 'custom', policy: ''},
  ];

  // Possible subtitle overrides.
  const defaultDescription = 'default description';
  const managedEnvironmentDescription =
      'disabled for managed environment description';
  const parentalControlDescription =
      'disabled for parental control description';

  /**
   * Checks that the radio buttons are shown and the toggle is properly
   * configured for showing the radio buttons.
   */
  function assertRadioButtonsShown() {
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.$$('cr-toggle').disabled);
    assertFalse(secureDnsRadioGroup.hidden);
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showSecureDnsSetting: true,
      secureDnsDescription: defaultDescription,
      secureDnsDisabledForManagedEnvironment: managedEnvironmentDescription,
      secureDnsDisabledForParentalControl: parentalControlDescription,
    });
  });

  setup(async function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    testBrowserProxy.setResolverList(resolverList);
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      dns_over_https: {
        mode: {value: settings.SecureDnsMode.AUTOMATIC},
        templates: {value: ''}
      },
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await test_util.flushTasks();
    secureDnsToggle = testElement.$$('#secureDnsToggle');
    secureDnsRadioGroup = testElement.$$('#secureDnsRadioGroup');
    assertRadioButtonsShown();
    assertEquals(
        testBrowserProxy.secureDnsSetting.mode, secureDnsRadioGroup.selected);
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsOff', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.OFF,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    Polymer.dom.flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.$$('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
  });

  test('SecureDnsAutomatic', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    Polymer.dom.flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
    assertEquals(
        settings.SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);
  });

  test('SecureDnsSecure', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.SECURE,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    Polymer.dom.flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
    assertEquals(settings.SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
  });

  test('SecureDnsManagedEnvironment', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.OFF,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.DISABLED_MANAGED,
    });
    Polymer.dom.flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$$('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(managedEnvironmentDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
    assertTrue(secureDnsToggle.$$('cr-policy-pref-indicator')
                   .$$('cr-tooltip-icon')
                   .hidden);
  });

  test('SecureDnsParentalControl', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.OFF,
      templates: [],
      managementMode:
          settings.SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS,
    });
    Polymer.dom.flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$$('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(parentalControlDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
    assertTrue(secureDnsToggle.$$('cr-policy-pref-indicator')
                   .$$('cr-tooltip-icon')
                   .hidden);
  });

  test('SecureDnsManaged', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: settings.SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    Polymer.dom.flush();
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$$('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.$$('cr-policy-pref-indicator'));
    assertFalse(secureDnsToggle.$$('cr-policy-pref-indicator')
                    .$$('cr-tooltip-icon')
                    .hidden);
  });
});
