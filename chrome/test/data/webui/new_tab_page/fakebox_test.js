// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertNotStyle, assertStyle, createTestProxy, NONE_ANIMATION} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageFakeboxTest', () => {
  /** @type {!FakeboxElement} */
  let fakebox;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;

    fakebox = document.createElement('ntp-fakebox');
    document.body.appendChild(fakebox);
  });

  test('when created shown and not focused', () => {
    // Assert.
    assertStyle(fakebox, 'visibility', 'visible');
    assertStyle(fakebox.$.hint, 'visibility', 'visible');
    assertStyle(fakebox.$.fakeCursor, 'visibility', 'hidden');
  });

  test('clicking voice search button send voice search event', async () => {
    // Arrange.
    const whenOpenVoiceSearch = eventToPromise('open-voice-search', fakebox);

    // Act.
    fakebox.$.voiceSearchButton.click();

    // Assert.
    await whenOpenVoiceSearch;
  });

  test('when focused fake cursor is blinking', async () => {
    // Act.
    testProxy.callbackRouterRemote.setFakeboxFocused(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(fakebox.$.fakeCursor, 'visibility', 'visible');
    assertNotStyle(fakebox.$.fakeCursor, 'animation', NONE_ANIMATION);
    assertStyle(fakebox.$.hint, 'visibility', 'hidden');
  });

  test('when blurred fake cursor is hidden', async () => {
    // Act.
    testProxy.callbackRouterRemote.setFakeboxFocused(false);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(fakebox.$.hint, 'visibility', 'visible');
    assertStyle(fakebox.$.fakeCursor, 'visibility', 'hidden');
  });

  test('when shown fakebox is visible', async () => {
    // Act.
    testProxy.callbackRouterRemote.setFakeboxVisible(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(fakebox, 'visibility', 'visible');
  });

  test('when hidden fakebox is invisible', async () => {
    // Act.
    testProxy.callbackRouterRemote.setFakeboxVisible(false);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(fakebox, 'visibility', 'hidden');
  });

  test('on mouse down focuses omnibox', async () => {
    // Act.
    fakebox.$.input.dispatchEvent(new PointerEvent('pointerdown'));

    // Assert.
    await testProxy.handler.whenCalled('focusOmnibox');
  });

  test('on paste pastes into omnibox', async () => {
    // Arrage.
    const clipboardData = new DataTransfer();
    clipboardData.setData('text/plain', 'foo');

    // Act.
    fakebox.$.input.dispatchEvent(
        new ClipboardEvent('paste', {clipboardData: clipboardData}));

    // Assert.
    const text = await testProxy.handler.whenCalled('pasteIntoOmnibox');
    assertEquals(text, 'foo');
  });

  test('on drag enter shows drag cursor', () => {
    // Act.
    fakebox.$.input.dispatchEvent(new DragEvent('dragenter'));

    // Assert.
    assertStyle(fakebox.$.fakeCursor, 'visibility', 'visible');
    assertStyle(fakebox.$.hint, 'visibility', 'hidden');
    assertStyle(fakebox.$.fakeCursor, 'animation', NONE_ANIMATION);
  });

  test('on drag leave hides drag cursor', () => {
    // Act.
    fakebox.$.input.dispatchEvent(new DragEvent('dragleave'));

    // Assert.
    assertStyle(fakebox.$.fakeCursor, 'visibility', 'hidden');
  });

  test('on drop pastes into and focuses omnibox', async () => {
    // Arrage.
    const dragData = new DataTransfer();
    dragData.setData('text/plain', 'foo');

    // Act.
    fakebox.$.input.dispatchEvent(
        new DragEvent('drop', {dataTransfer: dragData}));

    // Assert.
    await testProxy.handler.whenCalled('focusOmnibox');
    const text = await testProxy.handler.whenCalled('pasteIntoOmnibox');
    assertEquals(text, 'foo');
  });
});
