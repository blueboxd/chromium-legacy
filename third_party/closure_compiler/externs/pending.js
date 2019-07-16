// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 * @externs
 */

/**
 * TODO(dstockwell): Remove this once it is added to Closure Compiler itself.
 * @see https://drafts.fxtf.org/geometry/#DOMMatrix
 */
class DOMMatrix {
  /**
   * @param {number} x
   * @param {number} y
   */
  translateSelf(x, y) {}
  /**
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  rotateSelf(x, y, z) {}
  /**
   * @param {number} x
   * @param {number} y
   */
  scaleSelf(x, y) {}
  /**
   * @param {{x: number, y: number}} point
   * @return {{x: number, y: number}}
   */
  transformPoint(point) {}
}

/**
 * @see https://wicg.github.io/ResizeObserver/#resizeobserverentry
 * @typedef {{contentRect: DOMRectReadOnly,
 *            target: Element}}
 * TODO(dpapad): Remove this once it is added to Closure Compiler itself.
 */
let ResizeObserverEntry;

/**
 * @see https://wicg.github.io/ResizeObserver/#api
 * TODO(dpapad): Remove this once it is added to Closure Compiler itself.
 */
class ResizeObserver {
  /**
   * @param {!function(Array<ResizeObserverEntry>, ResizeObserver)} callback
   */
  constructor(callback) {}

  disconnect() {}

  /** @param {Element} target */
  observe(target) {}

  /** @param {Element} target */
  unobserve(target) {}
}

/**
 * @see
 * https://www.polymer-project.org/2.0/docs/api/namespaces/Polymer.RenderStatus
 * Queue a function call to be run before the next render.
 * @param {!Element} element The element on which the function call is made.
 * @param {!function()} fn The function called on next render.
 * @param {...*} args The function arguments.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.RenderStatus.beforeNextRender = function(element, fn, args) {};

/**
 * @see
 * https://www.webcomponents.org/element/@polymer/iron-iconset-svg
 * Polymer iconset of SVGs.
 * @implements {Polymer.Iconset}
 * @constructor
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.IronIconsetSvg = function() {};

/**
 * Added to IronIconsetSvg in chromium.patch.
 * @param {string} iconName Name of the icon to apply.
 * @param {boolean} targetIsRTL Whether the target element is RTL.
 * @return {Element} Returns an installable clone of the SVG element
 *     matching `id`.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.IronIconsetSvg.prototype.createIcon = function(iconName, targetIsRTL) {};

/**
 * @see
 * https://polymer-library.polymer-project.org/2.0/api/elements/Polymer.DomIf
 * @constructor
 */
Polymer.DomIf = function() {};

/**
 * @param {!HTMLTemplateElement} template
 * @return {!HTMLElement}
 * TODO(dpapad): Figure out if there is a better way to type-check Polymer2
 * while still using legacy Polymer1 syntax.
 */
Polymer.DomIf._contentForTemplate = function(template) {};

/**
 * @see
 * https://github.com/tc39/proposal-bigint
 * This supports wrapping and operating on arbitrarily large integers.
 *
 * @param {number} value
 */
let BigInt = function(value) {};

/** @const {!Clipboard} */
Navigator.prototype.clipboard;

/**
 * TODO(katie): Remove this once length is added to the Closure
 * chrome_extensions.js.
 * An event from the TTS engine to communicate the status of an utterance.
 * @constructor
 */
function TtsEvent() {}

/** @type {number} */
TtsEvent.prototype.length;



/**
 * @param {number|ArrayBufferView|Array.<number>|ArrayBuffer} length or array
 *     or buffer
 * @param {number=} opt_byteOffset
 * @param {number=} opt_length
 * @extends {ArrayBufferView}
 * @constructor
 * @noalias
 * @throws {Error}
 * @modifies {arguments}
 */
function BigInt64Array(length, opt_byteOffset, opt_length) {}

/** @type {number} */
BigInt64Array.BYTES_PER_ELEMENT;

/** @type {number} */
BigInt64Array.prototype.BYTES_PER_ELEMENT;

/** @type {number} */
BigInt64Array.prototype.length;

/**
 * @param {ArrayBufferView|Array.<number>} array
 * @param {number=} opt_offset
 */
BigInt64Array.prototype.set = function(array, opt_offset) {};

/**
 * @param {number} begin
 * @param {number=} opt_end
 * @return {!BigInt64Array}
 * @nosideeffects
 */
BigInt64Array.prototype.subarray = function(begin, opt_end) {};
