// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js'
]);

GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/mock_feedback.js'
]);

/**
 * Test fixture for Panel.
 */
ChromeVoxPanelTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  testGenCppIncludes() {
    ChromeVoxE2ETest.prototype.testGenCppIncludes.call(this);
  }

  /**
   * @return {!MockFeedback}
   */
  createMockFeedback() {
    const mockFeedback =
        new MockFeedback(this.newCallback(), this.newCallback.bind(this));
    mockFeedback.install();
    return mockFeedback;
  }

  getPanelWindow() {
    let panelWindow = null;
    while (!panelWindow) {
      panelWindow = chrome.extension.getViews().find(function(view) {
        return view.location.href.indexOf('chromevox/panel/panel.html') > 0;
      });
    }
    return panelWindow;
  }

  /**
   * Gets the Panel object in the panel.html window. Note that the extension
   * system destroys our reference to this object unpredictably so always ask
   * chrome.extension.getViews for it.
   */
  getPanel() {
    return this.getPanelWindow().Panel;
  }

  fireMockEvent(key) {
    return function() {
      const obj = {};
      obj.preventDefault = function() {};
      obj.stopPropagation = function() {};
      obj.key = key;
      this.getPanel().onKeyDown(obj);
    }.bind(this);
  }

  fireMockQuery(query) {
    return function() {
      const evt = {};
      evt.target = {};
      evt.target.value = query;
      this.getPanel().onSearchBarQuery(evt);
    }.bind(this);
  }

  get linksDoc() {
    return `
      <p>start</p>
      <a href="#">apple</a>
      <a href="#">grape</a>
      <a href="#">banana</a>
    `;
  }
};

TEST_F('ChromeVoxPanelTest', 'ActivateMenu', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksDoc, function(root) {
    const openMenus = new PanelCommand(PanelCommandType.OPEN_MENUS);
    mockFeedback.call(openMenus.send.bind(openMenus))
        .expectSpeech('Search the menus', 'Search')
        .call(this.fireMockEvent('ArrowRight'))
        .expectSpeech(
            'Jump', 'Menu',
            'Go To Beginning Of Table Search+Alt+Shift+Left arrow',
            / 1 of [0-9]{2} /)
        .call(this.fireMockEvent('ArrowRight'))
        .expectSpeech(
            'Speech', 'Menu',
            'Announce Current Battery Status Search+O, then B',
            / 1 of [0-9]{2} /)
        .replay();
  });
});

TEST_F('ChromeVoxPanelTest', 'LinkMenu', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksDoc, function(root) {
    mockFeedback.call(() => CommandHandler.onCommand('showLinksList'))
        .expectSpeech(
            'Link',
            'Menu',
            'apple Link',
            'Menu item',
            ' 1 of 3 ',
            )
        .call(this.fireMockEvent('ArrowLeft'))
        .expectSpeech('Landmark', 'Menu', 'No items', 'Menu item', ' 1 of 1 ')
        .call(this.fireMockEvent('ArrowRight'))
        .expectSpeech('Link', 'Menu', 'apple Link', 'Menu item', ' 1 of 3 ')
        .call(this.fireMockEvent('ArrowUp'))
        .expectSpeech('banana Link', 'Menu item', ' 3 of 3 ')
        .clearPendingOutput()
        .call(this.fireMockEvent('Enter'))
        .expectSpeech('banana', 'Link')
        .replay();
  });
});

TEST_F('ChromeVoxPanelTest', 'FormControlsMenu', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `<button>Cancel</button><button>OK</button>`, function(root) {
        mockFeedback.call(() => CommandHandler.onCommand('nextObject'))
            .expectSpeech('OK', 'Button')
            .call(() => CommandHandler.onCommand('showFormsList'))
            .expectSpeech(
                'Form Controls',
                'Menu',
                'OK Button',
                'Menu item',
                /2 of /,
                )
            .call(this.fireMockEvent('ArrowUp'))
            .expectSpeech('Cancel Button', 'Menu item', /1 of/)
            .replay();
      });
});

TEST_F('ChromeVoxPanelTest', 'SearchMenu', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.linksDoc, function(root) {
    const openMenus = new PanelCommand(PanelCommandType.OPEN_MENUS);
    mockFeedback.call(openMenus.send.bind(openMenus))
        .expectSpeech(
            'Search the menus', 'Search',
            'Type to search the menus. Use the up and down arrows to cycle' +
                ' through results. Use the left and right arrows to adjust' +
                ' the text caret, and to move between menus.');
    // Enter query into search box. ChromeVox should announce the first result.
    mockFeedback.call(this.fireMockQuery('announce'))
        .expectSpeech(/announce/i, 'Menu item', /1 of [0-9]+/);
    // Using ArrowUp and ArrowDown should navigate through results.
    mockFeedback.call(this.fireMockEvent('ArrowDown'))
        .expectSpeech(/announce/i, 'Menu item', /2 of [0-9]+/);
    mockFeedback.call(this.fireMockEvent('ArrowDown'))
        .expectSpeech(/announce/i, 'Menu item', /3 of [0-9]+/);
    mockFeedback.call(this.fireMockEvent('ArrowUp'))
        .expectSpeech(/announce/i, 'Menu item', /2 of [0-9]+/);
    mockFeedback.replay();
  });
});