// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input.
 */

suite('SettingsSecureDnsInput', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SecureDnsInputElement} */
  let testElement;

  // Possible error messages
  const invalidFormat = 'invalid format description';

  suiteSetup(function() {
    loadTimeData.overrideValues({
      secureDnsCustomFormatError: invalidFormat,
    });
  });

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('secure-dns-input');
    document.body.appendChild(testElement);
    Polymer.dom.flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsInputError', async function() {
    const crInput = testElement.$$('#input');
    assertFalse(crInput.invalid);
    assertEquals('', testElement.value);

    // Trigger validation on an empty input.
    testBrowserProxy.setIsEntryValid(false);
    testElement.validate();
    await testBrowserProxy.whenCalled('validateCustomDnsEntry');
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());

    // Enter a valid input and trigger validation.
    testElement.value = 'https://example.server/dns-query';
    testBrowserProxy.setIsEntryValid(true);
    testElement.validate();
    await testBrowserProxy.whenCalled('validateCustomDnsEntry');
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());

    // Enter an invalid input and trigger validation.
    testElement.value = 'invalid_template';
    testBrowserProxy.setIsEntryValid(false);
    testElement.validate();
    await testBrowserProxy.whenCalled('validateCustomDnsEntry');
    assertTrue(crInput.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(invalidFormat, crInput.errorMessage);

    // Trigger an input event and check that the error clears.
    crInput.fire('input');
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
    assertEquals('invalid_template', testElement.value);
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
