// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
// #import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-toast-manager', () => {
  let toastManager;

  suiteSetup(() => {
    PolymerTest.clearBody();
    toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('getToastManager', () => {
    assertEquals(toastManager, cr.toastManager.getToastManager());
  });

  test('simple show/hide', () => {
    assertFalse(toastManager.isToastOpen);
    toastManager.show('test');
    assertEquals('test', toastManager.$.content.textContent);
    assertTrue(toastManager.isToastOpen);
    toastManager.hide();
    assertFalse(toastManager.isToastOpen);
  });

  test('showForStringPieces', () => {
    const pieces = [
      {value: '\'', collapsible: false},
      {value: 'folder', collapsible: true},
      {value: '\' copied', collapsible: false},
    ];
    toastManager.showForStringPieces(pieces);
    const elements = toastManager.$.content.querySelectorAll('span');
    assertEquals(3, elements.length);
    assertFalse(elements[0].classList.contains('collapsible'));
    assertTrue(elements[1].classList.contains('collapsible'));
    assertFalse(elements[2].classList.contains('collapsible'));
  });

  test('duration passed through to toast', () => {
    toastManager.duration = 3;
    assertEquals(3, toastManager.$.toast.duration);
  });
});
