// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Private property symbols
// TODO(tkent): Use private fields.
const _value = Symbol();
const _trackElement = Symbol();
const _fillElement = Symbol();
const _slotElement = Symbol();

// Returns a function returning a CSSStyleSheet().
// TODO(tkent): Share this stylesheet factory feature with elements/toast/.
function styleSheetFactory() {
  let styleSheet;
  return () => {
    if (!styleSheet) {
      styleSheet = new CSSStyleSheet();
      styleSheet.replaceSync(`
.track {
  display: inline-block;
  inline-size: 10em;
  block-size: 1em;
  border: 1px solid black;
  box-sizing: border-box;
  overflow: hidden;
}

.trackFill {
  display: inline-block;
  background: red;
  block-size: 100%;
  inline-size: 0%;
  border-radius: 0;
  vertical-align: top;
  box-sizing: border-box;
  box-shadow: none;
  transition: all linear 0.1s;
}`);
    }
    return styleSheet;
  };
}

export class SwitchTrack {

  /**
   * @param {!Document} factory A factory for elements created for this track.
   */
  constructor(factory) {
    this[_value] = false;
    this._initializeDOM(factory);
  }

  /**
   * @return {!Element}
   */
  get element() {
    return this[_trackElement];
  }

  /**
   * @return {!CSSStyleSheet}
   */
  get styleSheet() {
    return styleSheetFactory()();
  }

  /**
   * @param {Boolean} newValue
   */
  set value(newValue) {
    let oldValue = this[_value];
    this[_value] = Boolean(newValue);

    let bar = this[_fillElement];
    if (bar) {
      bar.style.inlineSize = this[_value] ? '100%' : '0%';
      if (oldValue != this[_value]) {
        this._addSlot();
      }
    }
  }

  // TODO(tkent): Use private fields. #initializeDOM = factory => { ...};
  /**
   * @param {!Document} factory A factory for elements created for this track.
   */
  _initializeDOM(factory) {
    this[_trackElement] = factory.createElement('div');
    this[_trackElement].classList.add('track');
    this[_fillElement] = factory.createElement('span');
    this[_fillElement].classList.add('trackFill');
    this[_trackElement].appendChild(this[_fillElement]);
    this[_slotElement] = factory.createElement('slot');
    this._addSlot();
  }

  /**
   * Add the <slot>
   *   - next to _fillElement if _value is true
   *   - as a child of _fillElement if _value is false
   * This behavior is helpful to show text in the track.
   */
  _addSlot() {
    if (this[_value]) {
      this[_fillElement].appendChild(this[_slotElement]);
    } else {
      this[_trackElement].appendChild(this[_slotElement]);
    }
  }
}
