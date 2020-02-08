// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox Annotations UI.
 */

goog.provide('AnnotationsUI');

AnnotationsUI = class {
  /**
   * @param {Element} input The annotations input in the panel.
   * @param {!string} identifier A stringified NodeIdentifier.
   */
  static init(input, identifier) {
    AnnotationsUI.instance = new AnnotationsUI(identifier);
    input.focus();
    input.addEventListener('keydown', AnnotationsUI.onKeyDown, false);
  }

  /**
   * @param {!string} identifier A stringified NodeIdentifier.
   * @private
   */
  constructor(identifier) {
    this.identifier_ = identifier;
  }

  /**
   * @param {Event} event
   */
  static onKeyDown(event) {
    switch (event.key) {
      case 'Enter':
        AnnotationsUI.saveAnnotation(event.target.value);
        break;
      default:
        return;
    }
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Revives |AnnotationsUI.instance.identifier_| and sets its annotation to
   * |annotation|, if it's not empty.
   * @param {string} annotation
   */
  static saveAnnotation(annotation) {
    // Revive identifier.
    const identifier =
        NodeIdentifier.constructFromString(AnnotationsUI.instance.identifier_);
    chrome.extension.getBackgroundPage()['UserAnnotationHandler']
        .setAnnotationForIdentifier(identifier, annotation);
    Panel.closeMenusAndRestoreFocus();
  }
};
