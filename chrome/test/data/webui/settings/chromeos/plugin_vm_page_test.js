// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.PluginVmBrowserProxy} */
class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluginVmSharedPathsDisplayText',
      'removePluginVmSharedPath',
      'removePluginVm',
      'requestPluginVmInstallerView',
    ]);
  }

  /** override */
  getPluginVmSharedPathsDisplayText(paths) {
    this.methodCalled('getPluginVmSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** override */
  removePluginVmSharedPath(vmName, path) {
    this.methodCalled('removePluginVmSharedPath', [vmName, path]);
  }

  /** override */
  removePluginVm() {
    this.methodCalled('removePluginVm');
  }

  /** override */
  requestPluginVmInstallerView() {
    this.methodCalled('requestPluginVmInstallerView');
  }
}

/** @type {?TestPluginVmBrowserProxy} */
let pluginVmBrowserProxy = null;

suite('PluginVmPage', function() {
  /** @type {?SettingsPluginVmElement} */
  let page = null;

  /** @type {Array<string>} */
  let routes;
  /** @type {!Object} */
  let preTestSettingsRoutes;
  /** @type {!function(string)} */
  let preTestRouterNavigateTo;

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();

    routes = [];

    preTestRouterNavigateTo = settings.Router.getInstance().navigateTo;
    settings.Router.getInstance().navigateTo = (route) => routes.push(route);

    preTestSettingsRoutes = settings.routes;
    settings.routes = {PLUGIN_VM_DETAILS: 'TEST_PLUGIN_VM_DETAILS_ROUTE'};
  });

  teardown(function() {
    page.remove();

    settings.Router.getInstance().navigateTo = preTestRouterNavigateTo;

    settings.routes = preTestSettingsRoutes;
  });

  function setUpPluginVmPage({allowedByPolicy, vmImageExists}) {
    loadTimeData.overrideValues({allowPluginVm: allowedByPolicy});

    page = document.createElement('settings-plugin-vm-page');
    page.prefs = {plugin_vm: {image_exists: {value: vmImageExists}}};
    document.body.appendChild(page);

    Polymer.dom.flush();
  }

  test('PolicyEnabledAndImageExists', function() {
    setUpPluginVmPage({allowedByPolicy: true, vmImageExists: true});

    assertEquals(page.$$('#enable'), null);

    assertDeepEquals(routes, []);
    page.$$('#navigate-to-subpage').click();
    assertDeepEquals(routes, ['TEST_PLUGIN_VM_DETAILS_ROUTE']);
  });

  test('PolicyEnabledAndImageDoesntExist', function() {
    setUpPluginVmPage({allowedByPolicy: true, vmImageExists: false});

    assertEquals(page.$$('#navigate-to-subpage'), null);

    assertEquals(
        0, pluginVmBrowserProxy.getCallCount('requestPluginVmInstallerView'));
    page.$$('#enable').click();
    assertEquals(
        1, pluginVmBrowserProxy.getCallCount('requestPluginVmInstallerView'));
  });

  test('PolicyDisabledAndImageExists', function() {
    setUpPluginVmPage({allowedByPolicy: false, vmImageExists: true});

    assertEquals(page.$$('#enable'), null);

    assertDeepEquals(routes, []);
    page.$$('#navigate-to-subpage').click();
    assertDeepEquals(routes, ['TEST_PLUGIN_VM_DETAILS_ROUTE']);
  });

  test('PolicyDisabledAndImageDoesntExist', function() {
    setUpPluginVmPage({allowedByPolicy: false, vmImageExists: false});

    assertEquals(page.$$('#navigate-to-subpage'), null);

    assertTrue(page.$$('#enable').disabled);
  });
});

suite('Details', function() {
  /** @type {?SettingsPluginVmSubpageElement} */
  let page = null;

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-subpage');
    page.prefs = {
      plugin_vm: {
        image_exists: {value: true},
        printers_allowed: {value: false},
      }
    };
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Sanity', function() {
    assertTrue(!!page.$$('#plugin-vm-shared-paths'));
  });

  test('PrintingToggle', async function() {
    const toggle = page.$$('#plugin-vm-printer-access');
    assertTrue(!!toggle);
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
    assertFalse(toggle.pref.value);
    toggle.click();
    assertTrue(toggle.checked);
    assertTrue(toggle.pref.value);
  });
});

suite('SharedPaths', function() {
  /** @type {?SettingsPluginVmSharedPathsElement} */
  let page = null;

  function setPrefs(sharedPaths) {
    pluginVmBrowserProxy.resetResolver('getPluginVmSharedPathsDisplayText');
    page.prefs = {
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      }
    };
    return pluginVmBrowserProxy.whenCalled('getPluginVmSharedPathsDisplayText')
        .then(() => {
          Polymer.dom.flush();
        });
  }

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-shared-paths');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Remove', function() {
    return setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']})
        .then(() => {
          assertEquals(
              2, page.shadowRoot.querySelectorAll('.settings-box').length);
          assertEquals(
              2, page.shadowRoot.querySelectorAll('.list-item').length);
          assertFalse(page.$.pluginVmInstructionsRemove.hidden);
          assertTrue(!!page.$$('.list-item cr-icon-button'));

          // Remove first shared path, still one left.
          page.$$('.list-item cr-icon-button').click();
          return pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
        })
        .then(([vmName, path]) => {
          assertEquals('PvmDefault', vmName);
          assertEquals('path1', path);
          return setPrefs({'path2': ['PvmDefault']});
        })
        .then(() => {
          assertEquals(
              1, page.shadowRoot.querySelectorAll('.list-item').length);
          assertFalse(page.$.pluginVmInstructionsRemove.hidden);

          // Remove remaining shared path, none left.
          pluginVmBrowserProxy.resetResolver('removePluginVmSharedPath');
          page.$$('.list-item cr-icon-button').click();
          return pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
        })
        .then(([vmName, path]) => {
          assertEquals('PvmDefault', vmName);
          assertEquals('path2', path);
          return setPrefs({'ignored': ['ignore']});
        })
        .then(() => {
          assertEquals(
              0, page.shadowRoot.querySelectorAll('.list-item').length);
          // Verify remove instructions are hidden.
          assertTrue(page.$.pluginVmInstructionsRemove.hidden);
        });
  });
});

suite('Remove', function() {
  let page;
  const getDialog = () => page.$$(
      '#plugin-vm-remove settings-plugin-vm-remove-confirmation-dialog');
  const hasDialog = () => !!getDialog();

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-subpage');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Remove', async function() {
    assertFalse(hasDialog());

    page.$.pluginVmRemoveButton.click();
    assertFalse(hasDialog());

    Polymer.dom.flush();
    assertTrue(hasDialog());

    getDialog().$.continue.click();
    assertEquals(1, pluginVmBrowserProxy.getCallCount('removePluginVm'));

    await test_util.eventToPromise('dom-change', page.$$('#plugin-vm-remove'));
    assertFalse(hasDialog());

    assertEquals(getDeepActiveElement(), page.$.pluginVmRemoveButton);
  });

  test('RemoveDialogCancelled', async function() {
    assertFalse(hasDialog());

    page.$.pluginVmRemoveButton.click();
    assertFalse(hasDialog());

    Polymer.dom.flush();
    assertTrue(hasDialog());

    getDialog().$.cancel.click();
    assertTrue(hasDialog());
    assertEquals(0, pluginVmBrowserProxy.getCallCount('removePluginVm'));

    await test_util.eventToPromise('dom-change', page.$$('#plugin-vm-remove'));
    assertFalse(hasDialog());
    assertEquals(0, pluginVmBrowserProxy.getCallCount('removePluginVm'));

    assertEquals(getDeepActiveElement(), page.$.pluginVmRemoveButton);
  });
});
