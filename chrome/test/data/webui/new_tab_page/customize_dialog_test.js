// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/customize_dialog.js';

import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeDialogTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  setup(async () => {
    PolymerTest.clearBody();

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
    await flushTasks();
  });

  test('creating customize dialog opens cr dialog', () => {
    // Assert.
    assertTrue(customizeDialog.$.dialog.open);
  });

  test('background page selected at start', () => {
    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(shownPages[0].getAttribute('page-name'), 'backgrounds');
  });

  test('selecting menu item shows page', async () => {
    // Act.
    customizeDialog.$.menu.select('themes');
    await flushTasks();

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(shownPages[0].getAttribute('page-name'), 'themes');
  });
});
