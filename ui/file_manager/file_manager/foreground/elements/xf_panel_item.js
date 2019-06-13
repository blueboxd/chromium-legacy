// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A panel to display the status or progress of a file operation.
 */
class PanelItem extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));

    /** @private {Element} */
    this.indicator_ = this.shadowRoot.querySelector('#indicator');
  }

  /**
   * Static getter for the custom element template.
   * @private
   * @return {string}
   */
  static get template_() {
    return `<link rel='stylesheet' href='files_visual_signals.css'>
                <div class='xf-panel-item'>
                    <div class='xf-panel-text'>
                        <span class='xf-panel-label-text'>
                                Placeholder text</span>
                        <br/>
                    </div>
                    <div class='xf-padder-24'></div>
                    <button class='xf-button' src='pause-icon.png'
                            tabindex='-1'>
                    </button>
                    <div class='xf-padder-4'></div>
                    <button class='xf-button' src='cancel-icon.png'
                            tabindex='-1'>
                    </button>
                    <div class='xf-padder-16'></div>
                </div>`;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'indicator', 'progress', 'status', 'primary-text', 'secondary-text'
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {string} oldValue Old value of the attribute.
   * @param {string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    /** @type {HTMLElement} */
    let indicator;
    /** @type {HTMLSpanElement} */
    let textNode;
    if (oldValue === newValue) {
      return;
    }
    switch (name) {
      case 'indicator':
        // Get rid of any existing indicator.
        const oldIndicator = this.shadowRoot.querySelector('#indicator');
        if (oldIndicator) {
          oldIndicator.remove();
        }
        switch (newValue) {
          default:  // Always set something so the panel doesn't shrink.
          case 'progress':
          case 'largeprogress':
            indicator = document.createElement('xf-circular-progress');
            if (newValue === 'largeprogress') {
              indicator.setAttribute('radius', '14');
            } else {
              indicator.setAttribute('radius', '10');
            }
            break;
          case 'status':
            indicator = document.createElement('xf-activity-complete');
            const status = this.getAttribute('status');
            if (status) {
              indicator.status = status;
            }
            break;
        }
        if (indicator) {
          const itemRoot = this.shadowRoot.querySelector('.xf-panel-item');
          indicator.setAttribute('id', 'indicator');
          itemRoot.prepend(indicator);
        }
        break;
      case 'progress':
        indicator = this.shadowRoot.querySelector('xf-circular-progress');
        if (indicator) {
          indicator.progress = Number(newValue);
        }
        break;
      case 'status':
        indicator = this.shadowRoot.querySelector('#indicator');
        if (indicator) {
          indicator.status = newValue;
        }
        break;
      case 'primary-text':
        textNode = this.shadowRoot.querySelector('.xf-panel-label-text');
        if (textNode) {
          textNode.textContent = newValue;
        }
        break;
      case 'secondary-text':
        textNode = this.shadowRoot.querySelector('.xf-panel-secondary-text');
        if (!textNode) {
          const parent = this.shadowRoot.querySelector('.xf-panel-text');
          if (!parent) {
            return;
          }
          textNode = document.createElement('span');
          textNode.setAttribute('class', 'xf-panel-secondary-text');
          parent.appendChild(textNode);
        }
        // Remove the secondary text node if the text is empty
        if (newValue == '') {
          textNode.remove();
        } else {
          textNode.textContent = newValue;
        }
        break;
    }
  }

  /**
   * Setter to set the indicator type.
   * @param {string} indicator Progress (optionally large) or status.
   */
  set indicator(indicator) {
    // Reflect the status property into the attribute.
    this.setAttribute('indicator', indicator);
  }

  /**
   * Setter to set the success/failure indication.
   * @param {string} status Status value being set.
   */
  set status(status) {
    // Reflect the status property into the attribute.
    this.setAttribute('status', status);
  }

  /**
   * Setter to set the progress property, sent to any child indicator.
   * @param {string} progress Progress value being set.
   * @public
   */
  set progress(progress) {
    // Reflect the progress property into the attribute.
    this.setAttribute('progress', progress);
  }

  /**
   * Setter to set the primary text on the panel.
   * @param {string} text Text to be shown.
   */
  set primaryText(text) {
    // Reflect the status property into the attribute.
    this.setAttribute('primary-text', text);
  }

  /**
   * Setter to set the secondary text on the panel.
   * @param {string} text Text to be shown.
   */
  set secondaryText(text) {
    // Reflect the status property into the attribute.
    this.setAttribute('secondary-text', text);
  }
}

window.customElements.define('xf-panel-item', PanelItem);
