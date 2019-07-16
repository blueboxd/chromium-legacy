/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview This file defines virtual-scroller element.
 * EXPLAINER: https://github.com/fergald/virtual-scroller/blob/master/README.md
 * TEST PATH: third_party/blink/web_tests/http/tests/virtual-scroller/*
 *            third_party/blink/web_tests/wpt_internal/virtual-scroller/*
 * @package
 */
import {VisibilityManager} from './visibility-manager.mjs';

function styleSheetFactory() {
  let styleSheet;
  return () => {
    if (!styleSheet) {
      styleSheet = new CSSStyleSheet();
      styleSheet.replaceSync(`
:host {
  display: block;
}

::slotted(*) {
  display: block !important;
  contain: layout style;
}
`);
    }
    return styleSheet;
  };
}

/**
 * The class backing the virtual-scroller custom element.
 */
export class VirtualScrollerElement extends HTMLElement {
  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'closed'});
    shadowRoot.adoptedStyleSheets = [styleSheetFactory()()];
    shadowRoot.appendChild(document.createElement('slot'));

    const visibilityManager = new VisibilityManager(this.children);

    new ResizeObserver(() => {
      visibilityManager.scheduleSync();
    }).observe(this);

    new MutationObserver(records => {
      visibilityManager.applyMutationObserverRecords(records);
    }).observe(this, {childList: true});
  }
}

customElements.define('virtual-scroller', VirtualScrollerElement);
