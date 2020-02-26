// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for settings-secure-dns. */
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
    {
      name: 'resolver1',
      value: 'resolver1_template',
      policy: 'https://resolver1_policy.com/'
    },
    {
      name: 'resolver2',
      value: 'resolver2_template',
      policy: 'https://resolver2_policy.com/'
    },
    {
      name: 'resolver3',
      value: 'resolver3_template',
      policy: 'https://resolver3_policy.com/'
    },
  ];

  /**
   * Checks that the radio buttons are shown and the toggle is properly
   * configured for showing the radio buttons.
   */
  function assertRadioButtonsShown() {
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.hasAttribute('disabled'));
    assertFalse(secureDnsRadioGroup.hidden);
  }

  setup(async function() {
    loadTimeData.overrideValues({showSecureDnsSetting: true});
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
    cr.webUIListenerCallback(
        'secure-dns-setting-changed',
        {mode: settings.SecureDnsMode.OFF, templates: []});
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.hasAttribute('disabled'));
    assertTrue(secureDnsRadioGroup.hidden);
  });

  test('SecureDnsAutomatic', function() {
    cr.webUIListenerCallback(
        'secure-dns-setting-changed',
        {mode: settings.SecureDnsMode.AUTOMATIC, templates: []});
    assertRadioButtonsShown();
    assertEquals(
        settings.SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);
  });

  test('SecureDnsSecure', function() {
    cr.webUIListenerCallback(
        'secure-dns-setting-changed',
        {mode: settings.SecureDnsMode.SECURE, templates: []});
    assertRadioButtonsShown();
    assertEquals(settings.SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
  });

  test('SecureDnsModeChange', function() {
    // Start in secure mode.
    cr.webUIListenerCallback(
        'secure-dns-setting-changed',
        {mode: settings.SecureDnsMode.SECURE, templates: []});

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        settings.SecureDnsMode.OFF,
        testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to enable secure dns.
    secureDnsToggle.click();
    assertEquals(
        settings.SecureDnsMode.SECURE,
        testElement.prefs.dns_over_https.mode.value);

    // Change the radio button to automatic mode.
    secureDnsRadioGroup.querySelector('cr-radio-button').click();
    assertEquals(
        settings.SecureDnsMode.AUTOMATIC,
        testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        settings.SecureDnsMode.OFF,
        testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to enable secure dns.
    secureDnsToggle.click();
    assertEquals(
        settings.SecureDnsMode.AUTOMATIC,
        testElement.prefs.dns_over_https.mode.value);
  });

  test('SecureDnsDropdown', function() {
    const options =
        testElement.$$('#secureResolverSelect').querySelectorAll('option');
    assertEquals(4, options.length);

    for (let i = 0; i < options.length; i++) {
      assertEquals(resolverList[i].name, options[i].text);
      assertEquals(resolverList[i].value, options[i].value);
    }
  });

  test('SecureDnsDropdownCustom', function() {
    cr.webUIListenerCallback(
        'secure-dns-setting-changed',
        {mode: settings.SecureDnsMode.SECURE, templates: ['custom']});
    assertRadioButtonsShown();
    assertEquals(settings.SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(0, testElement.$$('#secureResolverSelect').selectedIndex);
    assertTrue(testElement.$$('#privacyPolicy').hasAttribute('hidden'));
  });

  test('SecureDnsDropdownChangeInSecureMode', function() {
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.SECURE,
      templates: [resolverList[1].value]
    });
    assertRadioButtonsShown();
    assertEquals(settings.SecureDnsMode.SECURE, secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$$('#secureResolverSelect');
    const privacyPolicyLine = testElement.$$('#privacyPolicy');

    assertEquals(1, dropdownMenu.selectedIndex);
    assertFalse(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals(
        resolverList[1].policy, privacyPolicyLine.querySelector('a').href);

    // Change to resolver2
    dropdownMenu.value = resolverList[2].value;
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals(2, dropdownMenu.selectedIndex);
    assertFalse(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals(
        resolverList[2].policy, privacyPolicyLine.querySelector('a').href);
    assertEquals(
        resolverList[2].value,
        testElement.prefs.dns_over_https.templates.value);

    // Change to custom
    dropdownMenu.value = 'custom';
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals(0, dropdownMenu.selectedIndex);
    assertTrue(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals('custom', testElement.prefs.dns_over_https.templates.value);
  });

  test('SecureDnsDropdownChangeInAutomaticMode', function() {
    testElement.prefs.dns_over_https.templates.value = 'resolver1_template';
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.AUTOMATIC,
      templates: [resolverList[1].value]
    });
    assertRadioButtonsShown();
    assertEquals(
        settings.SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$$('#secureResolverSelect');
    const privacyPolicyLine = testElement.$$('#privacyPolicy');

    // Select resolver3. This change should not be reflected in prefs.
    assertNotEquals(3, dropdownMenu.selectedIndex);
    dropdownMenu.value = resolverList[3].value;
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals(3, dropdownMenu.selectedIndex);
    assertFalse(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);
    assertEquals(
        'resolver1_template', testElement.prefs.dns_over_https.templates.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(
        settings.SecureDnsMode.OFF,
        testElement.prefs.dns_over_https.mode.value);
    assertEquals('', testElement.prefs.dns_over_https.templates.value);

    // Get another event enabling automatic mode.
    cr.webUIListenerCallback('secure-dns-setting-changed', {
      mode: settings.SecureDnsMode.AUTOMATIC,
      templates: [resolverList[1].value]
    });
    assertFalse(secureDnsRadioGroup.hidden);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertFalse(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);

    // Click on secure mode radio button.
    secureDnsRadioGroup.querySelectorAll('cr-radio-button')[1].click();
    assertFalse(secureDnsRadioGroup.hidden);
    assertEquals(settings.SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertFalse(privacyPolicyLine.hasAttribute('hidden'));
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);
    assertEquals(
        settings.SecureDnsMode.SECURE,
        testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        'resolver3_template', testElement.prefs.dns_over_https.templates.value);
  });
});
