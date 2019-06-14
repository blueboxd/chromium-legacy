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

    /**
     * TODO(crbug.com/947388) make this a closure enum.
     * @const
     */
    this.panelTypeDefault = -1;
    this.panelTypeProgress = 0;
    this.panelTypeSummary = 1;
    this.panelTypeDone = 2;
    this.panelTypeError = 3;
    this.panelTypeInfo = 4;

    /** @private {number} */
    this.panelType_ = this.panelTypeDefault;
  }

  /**
   * Static getter for the custom element template.
   * @private
   * @return {string}
   */
  static get template_() {
    return `<link rel='stylesheet'
              href='foreground/elements/files_xf_elements.css'>
                <div class='xf-panel-item'>
                    <xf-circular-progress id='indicator'>
                    </xf-circular-progress>
                    <div class='xf-panel-text'>
                        <span class='xf-panel-label-text'>
                                Placeholder text
                        </span>
                        <br/>
                    </div>
                    <div class='xf-padder-24'></div>
                    <xf-button id='secondary-action' tabindex='-1'>
                    </xf-button>
                    <div id='button-gap' class='xf-padder-4'></div>
                    <xf-button id='primary-action' tabindex='-1'>
                    </xf-button>
                    <div class='xf-padder-16'></div>
                </div>`;
  }

  /**
   * Remove an element from the panel using it's id.
   * @private
   */
  removePanelElementById_(id) {
    const element = this.shadowRoot.querySelector(id);
    if (element) {
      element.remove();
    }
  }

  /**
   * Sets up the different panel types. Panels have per-type configuration
   * templates, but can be further customized using individual attributes.
   * @param {number} type The enumerated panel type to set up.
   * @private
   */
  setPanelType(type) {
    if (this.panelType_ === type) {
      return;
    }

    // Remove the indicators/buttons that can change.
    this.removePanelElementById_('#indicator');
    this.removePanelElementById_('#primary-action');
    this.removePanelElementById_('#secondary-action');

    // Mark the indicator as empty so it recreates on setAttribute.
    this.setAttribute('indicator', 'empty');

    const buttonSpacer = this.shadowRoot.querySelector('#button-gap');

    // Setup the panel configuration for the panel type.
    // TOOD(crbug.com/947388) Simplify this switch breaking out common cases.
    switch (type) {
      case this.panelTypeProgress:
        this.setAttribute('indicator', 'progress');
        let primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.setAttribute('category', 'pause');
        buttonSpacer.insertAdjacentElement('beforebegin', primaryButton);

        let secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.setAttribute('category', 'cancel');
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case this.panelTypeSummary:
        this.setAttribute('indicator', 'largeprogress');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.setAttribute('category', 'expand');
        buttonSpacer.insertAdjacentElement('afterend', primaryButton);
        break;
      case this.panelTypeDone:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'success');
        primaryButton = document.createElement('xf-button');
        primaryButton.id = 'primary-action';
        primaryButton.setAttribute('category', 'dismiss');
        buttonSpacer.insertAdjacentElement('beforebegin', primaryButton);

        secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.setAttribute('category', 'undo');
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case this.panelTypeError:
        this.setAttribute('indicator', 'status');
        this.setAttribute('status', 'failure');
        secondaryButton = document.createElement('xf-button');
        secondaryButton.id = 'secondary-action';
        secondaryButton.setAttribute('category', 'retry');
        buttonSpacer.insertAdjacentElement('afterend', secondaryButton);
        break;
      case this.panelTypeInfo:
        break;
    }

    this.panelType_ = type;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'count',
      'indicator',
      'panel-type',
      'primary-text',
      'progress',
      'secondary-text',
      'status',
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    /** @type {HTMLElement} */
    let indicator = null;
    /** @type {HTMLSpanElement} */
    let textNode;
    if (oldValue === newValue) {
      return;
    }
    // TODO(adanilo) Chop out each attribute handler into a function.
    switch (name) {
      case 'count':
        if (this.indicator_) {
          this.indicator_.setAttribute('label', newValue);
        }
        break;
      case 'indicator':
        // Get rid of any existing indicator
        const oldIndicator = this.shadowRoot.querySelector('#indicator');
        if (oldIndicator) {
          oldIndicator.remove();
        }
        switch (newValue) {
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
        this.indicator_ = indicator;
        if (indicator) {
          const itemRoot = this.shadowRoot.querySelector('.xf-panel-item');
          indicator.setAttribute('id', 'indicator');
          itemRoot.prepend(indicator);
        }
        break;
      case 'panel-type':
        this.setPanelType(Number(newValue));
        if (this.parent && this.parent.updateSummaryPanel) {
          this.parent.updateSummaryPanel();
        }
        break;
      case 'progress':
        if (this.indicator_) {
          this.indicator_.progress = Number(newValue);
          if (this.parent && this.parent.updateProgress) {
            this.parent.updateProgress();
          }
        }
        break;
      case 'status':
        if (this.indicator_) {
          this.indicator_.status = newValue;
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
    this.setAttribute('indicator', indicator);
  }

  /**
   * Setter to set the success/failure indication.
   * @param {string} status Status value being set.
   */
  set status(status) {
    this.setAttribute('status', status);
  }

  /**
   * Setter to set the progress property, sent to any child indicator.
   * @param {string} progress Progress value being set.
   * @public
   */
  set progress(progress) {
    this.setAttribute('progress', progress);
  }

  /**
   *  Getter for the progress indicator percentage.
   */
  get progress() {
    return this.indicator_.progress || 0;
  }

  /**
   * Setter to set the primary text on the panel.
   * @param {string} text Text to be shown.
   */
  set primaryText(text) {
    this.setAttribute('primary-text', text);
  }

  /**
   * Setter to set the secondary text on the panel.
   * @param {string} text Text to be shown.
   */
  set secondaryText(text) {
    this.setAttribute('secondary-text', text);
  }

  /**
   * Setter to set the panel type.
   * @param {number} type Enum value for the panel type.
   */
  set panelType(type) {
    this.setAttribute('panel-type', type);
  }

  /**
   * Getter for the panel type.
   * TODO(crbug.com/947388) Add closure annotations to getters.
   */
  get panelType() {
    return this.panelType_;
  }

  /**
   * Getter for the primary action button.
   */
  get primaryButton() {
    return this.shadowRoot.querySelector('#primary-action');
  }

  /**
   * Getter for the secondary action button.
   */
  get secondaryButton() {
    return this.shadowRoot.querySelector('#secondary-action');
  }
}

window.customElements.define('xf-panel-item', PanelItem);
