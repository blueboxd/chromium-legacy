// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Array<{value:number, name:string}>} options
 * @param {number} value
 * @returns {boolean}
 */
function hasOptionWithValue(options, value) {
  return options.filter(o => o.value === value).length > 0;
}

suite('ManageAccessibilityPageTests', function() {
  let page = null;

  function initPage() {
    page = document.createElement('settings-switch-access-subpage');
    page.prefs = {
      settings: {
        a11y: {
          switch_access: {
            auto_scan: {
              enabled: {
                key: 'settings.a11y.switch_access.auto_scan.enabled',
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
                value: false
              }
            },
            next: {
              setting: {
                key: 'settings.a11y.switch_access.next.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
            previous: {
              setting: {
                key: 'settings.a11y.switch_access.previous.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
            select: {
              setting: {
                key: 'settings.a11y.switch_access.select.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
          }
        }
      }
    };

    document.body.appendChild(page);
  }

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
  });

  test(
      'Switch assignment option unavailable when assigned to another command',
      function() {
        initPage();
        const assignedValue = SwitchAccessAssignmentValue.ONE;

        // Check that the value is available in all three dropdowns.
        assertTrue(hasOptionWithValue(page.optionsForNext_, assignedValue));
        assertTrue(hasOptionWithValue(page.optionsForPrevious_, assignedValue));
        assertTrue(hasOptionWithValue(page.optionsForSelect_, assignedValue));

        // Assign the value to one setting (next).
        page.prefs.settings.a11y.switch_access.next.setting.value =
            assignedValue;
        page.updateOptionsForDropdowns_();

        // Check that the value no longer appears in the other two dropdowns
        assertTrue(hasOptionWithValue(page.optionsForNext_, assignedValue));
        assertFalse(
            hasOptionWithValue(page.optionsForPrevious_, assignedValue));
        assertFalse(hasOptionWithValue(page.optionsForSelect_, assignedValue));
      });
});
