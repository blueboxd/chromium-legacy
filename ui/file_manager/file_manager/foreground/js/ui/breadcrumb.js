// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const {string} breadCrumbTemplate
 */
const breadCrumbTemplate = `
  <style>
    :host([hidden]), [hidden] {
      display: none;
    }

    :host {
      display: inline-flex;
      font-family: 'Roboto Medium';
      font-size: 14px;
      outline: none;
      overflow: hidden;
      user-select: none;
      white-space: nowrap;
    }

    p {
      margin: 0;
    }

    p + p:before {
      content: '>';
      min-width: 20px;
      margin: 0;
      vertical-align: middle;
    }

    p[hidden] button[elider] {
      display: none;
    }

    button {
      /* don't use browser's background-color. */
      background-color: unset;
      border: 1px solid transparent;
      color: var(--google-grey-700);
      cursor: pointer;

      /* don't use browser's button font. */
      font: inherit;
      margin: 0;

      /* text rendering debounce: fix a minimum width. */
      min-width: 1.2em;

      /* elide wide text */
      max-width: 200px;
      overflow: hidden;
      padding: calc(8px - 1px);
      /* text rendering debounce: center. */
      text-align: center;
      text-overflow: ellipsis;
      vertical-align: middle;
    }

    button[disabled] {
      color: var(--google-grey-900);
      cursor: default;
      font-weight: 500;
    }

    button:focus {
      border: 1px solid blue;
      outline: none;
    }

    button:active {
      background-color: lightblue;
    }

    #elided, #elided button {  /* drop-down menu and buttons */
      display: none;
    }

    :host([checked]) #elided {
      background-color: white;
      border-radius: 2px;
      box-shadow: 0 1px 4px 0 rgba(0, 0, 0, 50%);
      display: block;
      margin-block-start: 0.2em;
      margin-inline-start: -0.2em;
      max-height: 40vh;
      padding: 8px 0;
      position: absolute;
      overflow: hidden auto;
      z-index: 502;
    }

    :host([checked]) #elided button {
      color: rgb(51, 51, 51);
      display: block;
      font-family: 'Roboto';
      font-size: 13px;
      min-width: 14em;  /* menu width */
      max-width: 14em;
      text-align: start;
    }
  </style>

  <p hidden><button id='first'></button></p>
  <p hidden><button elider></button></p>
  <p hidden><button id='second'></button></p>
  <p hidden><button id='third'></button></p>
  <p hidden><button id='fourth'></button></p>
`;

/**
 * Class breadCrumb.
 */
class breadCrumb extends HTMLElement {
  constructor() {
    /**
     * Create element content.
     */
    super().attachShadow({mode: 'open'}).innerHTML = breadCrumbTemplate;

    /**
     * User interaction signals callback.
     * @private @type {!function(*)}
     */
    this.signal_ = console.log;

    /**
     * BreadCrumb path parts.
     * @private @type {!Array<string>}
     */
    this.parts_ = [];
  }

  /**
   * Sets the user interaction signal callback.
   * @param {?function(*)} signal
   */
  setSignalCallback(signal) {
    this.signal_ = signal || console.log;
  }

  /**
   * DOM connected.
   * @private
   */
  connectedCallback() {
    this.onkeydown = this.onKeydown_.bind(this);
    this.onclick = this.onClicked_.bind(this);
    this.onblur = this.closeMenu_.bind(this);
    this.setAttribute('tabindex', '0');
  }

  /**
   * Get parts.
   * @return {!Array<string>}
   */
  get parts() {
    return this.parts_;
  }

  /**
   * Get path.
   * @return {string} path
   */
  get path() {
    return this.parts_.join('/');
  }

  /**
   * Sets the path: update parts from |path|. Emits a 'path-updated' _before_
   * updating the parts <button> element content to the new |path|.
   *
   * @param {string} path
   */
  set path(path) {
    this.parts_ = path ? path.split('/') : [];
    this.signal_('path-updated');
    this.renderParts_();
  }

  /**
   * Renders the path <button> parts. Emits 'path-rendered' signal.
   *
   * @private
   */
  renderParts_() {
    const buttons = this.shadowRoot.querySelectorAll('button[id]');
    const enabled = [];

    function setButton(i, text) {
      buttons[i].removeAttribute('has-tooltip');
      buttons[i].parentElement.hidden = !text;
      buttons[i].textContent = text;
      buttons[i].disabled = false;
      !!text && enabled.push(i);
    }

    const parts = this.parts_;
    setButton(0, parts.length > 0 ? parts[0] : null);
    setButton(1, parts.length == 4 ? parts[parts.length - 3] : null);
    buttons[1].hidden = parts.length != 4;
    setButton(2, parts.length > 2 ? parts[parts.length - 2] : null);
    setButton(3, parts.length > 1 ? parts[parts.length - 1] : null);

    if (enabled.length) {  // Disable the "last" button.
      buttons[enabled.pop()].disabled = true;
    }

    this.removeAttribute('checked');
    this.renderElidedParts_();

    this.setAttribute('path', this.path);
    this.signal_('path-rendered');
  }

  /**
   * Renders the elided parts of the path in a drop-down menu. Note the drop-
   * down is hidden, via its parent, if there are no elided parts.
   *
   * @private
   */
  renderElidedParts_() {
    const elider = this.shadowRoot.querySelector('button[elider]');
    const parts = this.parts_;

    let content = '...';
    elider.parentElement.hidden = parts.length <= 4;
    if (elider.parentElement.hidden) {
      elider.textContent = content;
      return;
    }

    content += '<div id="elided">';
    for (let i = 1; i < parts.length - 2; ++i) {
      content += `<button tabindex='-1'>${parts[i]}</button>`;
    }

    elider.innerHTML = content + '</div>';
  }

  /**
   * Returns the breadcrumb buttons: they contain the current path ordered by
   * its parts, which are stored in the <button>.textContent.
   *
   * @return {!NodeList}
   */
  getBreadcrumbButtons() {
    const parts = 'button:not([elider]):not([hidden])';
    return this.shadowRoot.querySelectorAll(parts);
  }

  /**
   * Returns the visible buttons rendered CSS overflow: ellipsis that have no
   * 'has-tooltip' attribute.
   *
   * Note: call in a requestAnimationFrame() to avoid a style resolve.
   *
   * @return {!Array<HTMLButtonElement>} buttons Callers can set the tool tip
   *    attribute on the returned buttons.
   */
  getEllipsisButtons() {
    return Array.from(this.getBreadcrumbButtons()).filter(button => {
      if (!button.hasAttribute('has-tooltip') && button.offsetWidth) {
        return button.offsetWidth < button.scrollWidth;
      }
    });
  }

  /**
   * Handles 'click' events.
   *
   * Emits an index signal on breadcumb button click: the index indicates the
   * current path part that was clicked. Drop-down menu clicks open and close
   * (toggle) the menu element.
   *
   * @param {Event} event
   * @private
   */
  onClicked_(event) {
    event.stopImmediatePropagation();
    event.preventDefault();

    if (event.repeat) {
      return;
    }

    const element = event.path[0];
    if (element.hasAttribute('elider')) {
      this.toggleMenu_();
      return;
    }

    this.closeMenu_();

    if (element instanceof HTMLButtonElement) {
      const parts = Array.from(this.getBreadcrumbButtons());
      this.signal_(parts.indexOf(element));
    }
  }

  /**
   * Handles keyboard events.
   *
   * @param {Event} event
   * @private
   */
  onKeydown_(event) {
    if (event.key === 'Tab') {
      this.closeMenu_();
    }

    if (event.key === ' ' || event.key === 'Enter') {
      this.onClicked_(event);
    }
  }

  /**
   * Toggles drop-down menu: opens if closed and emits 'path-rendered' signal
   * or closes if open via closeMenu_.
   *
   * @private
   */
  toggleMenu_() {
    if (!this.hasAttribute('checked')) {
      this.setAttribute('checked', '');
      this.signal_('path-rendered');
    } else {
      this.closeMenu_();
    }
  }

  /**
   * Closes drop-down menu if needed.
   *
   * @private
   */
  closeMenu_() {
    if (this.hasAttribute('checked')) {
      this.removeAttribute('checked');
    }
  }
}

customElements.define('bread-crumb', breadCrumb);
