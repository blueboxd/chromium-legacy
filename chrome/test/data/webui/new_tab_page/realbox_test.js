// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, decodeString16} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {assertStyle, createTestProxy, createTheme} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

/**
 * Helps track realbox browser call arguments.
 * @implements {newTabPage.mojom.PageHandlerRemote}
 * @extends {TestBrowserProxy}
 */
class TestRealboxBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['queryAutocomplete']);
  }

  queryAutocomplete(input, preventInlineAutocomplete) {
    this.methodCalled('queryAutocomplete', {input, preventInlineAutocomplete});
  }
}

/**
 * @param {string} type
 * @param {!Object=} modifiers
 */
function createTrustedEvent(type, modifiers = {}) {
  return Object.assign(
      {
        type,
        isTrusted: true,
        defaultPrevented: false,
        preventDefault() {
          this.defaultPrevented = true;
        },
      },
      modifiers);
}

suite('NewTabPageRealboxTest', () => {
  /** @type {!RealboxElement} */
  let realbox;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      realboxMatchOmniboxTheme: true,
    });
  });

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler = new TestRealboxBrowserProxy();
    BrowserProxy.instance_ = testProxy;

    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);
  });

  test('when created is shown and is not focused', () => {
    // Assert.
    assertFalse(realbox.hidden);
    assertNotEquals(realbox, getDeepActiveElement());
  });

  test('clicking voice search button send voice search event', async () => {
    // Arrange.
    const whenOpenVoiceSearch = eventToPromise('open-voice-search', realbox);

    // Act.
    realbox.$.voiceSearchButton.click();

    // Assert.
    await whenOpenVoiceSearch;
  });

  test('setting theme updates realbox', async () => {
    // Assert.
    assertStyle(realbox, '--search-box-bg', '');
    assertStyle(realbox, '--search-box-placeholder', '');
    assertStyle(realbox, '--search-box-results-bg', '');
    assertStyle(realbox, '--search-box-text', '');
    assertStyle(realbox, '--search-box-icon', '');

    // Arrange.
    realbox.theme = createTheme().searchBox;

    // Assert.
    assertStyle(realbox, '--search-box-bg', 'rgba(0, 0, 0, 255)');
    assertStyle(realbox, '--search-box-placeholder', 'rgba(0, 0, 3, 255)');
    assertStyle(realbox, '--search-box-results-bg', 'rgba(0, 0, 4, 255)');
    assertStyle(realbox, '--search-box-text', 'rgba(0, 0, 12, 255)');
    assertStyle(realbox, '--search-box-icon', 'rgba(0, 0, 1, 255)');
  });

  test('realbox default icon', async () => {
    // Assert.
    assertStyle(
        realbox.$.realboxIcon, '-webkit-mask-image',
        'url("chrome://new-tab-page/search.svg")');
    assertStyle(realbox.$.realboxIcon, 'background-image', 'none');

    // Arrange.
    loadTimeData.overrideValues({
      realboxDefaultIcon: 'google_g.png',
    });
    PolymerTest.clearBody();
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);

    // Assert.
    assertStyle(realbox.$.realboxIcon, '-webkit-mask-image', 'none');
    assertStyle(
        realbox.$.realboxIcon, 'background-image',
        'url("chrome://new-tab-page/google_g.png")');
  });

  test('left-clicking input when empty queries autocomplete', async () => {
    realbox.$.input.value = '';
    realbox.$.input.onmousedown(createTrustedEvent('mousedown', {button: 0}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    await testProxy.handler.whenCalled('queryAutocomplete').then((args) => {
      assertTrue(decodeString16(args.input) === '');
      assertFalse(args.preventInlineAutocomplete);
    });

    // Only left clicks query autocomplete.
    realbox.$.input.onmousedown(createTrustedEvent('mousedown', {button: 1}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    // Untrusted events won't qeury autocomplete.
    realbox.$.input.onmousedown(new MouseEvent('mousedown', {button: 0}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    // Non-empty input won't query autocomplete.
    realbox.$.input.value = '   ';
    realbox.$.input.onmousedown(createTrustedEvent('mousedown', {button: 0}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });
});
