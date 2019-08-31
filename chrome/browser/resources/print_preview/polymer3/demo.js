// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class HelloPolymer3Element extends PolymerElement {
  static get template() {
    return html`
      <style include="md-select">
        cr-toggle {
          display: inline-block;
        }
      </style>

      <cr-checkbox checked="{{checkboxChecked_}}">
        [[checkboxChecked_]]
      </cr-checkbox>

      <div>
        <cr-toggle checked="{{toggleChecked_}}"></cr-toggle>
        <span>[[toggleChecked_]]</span>
      </div>

      <select class="md-select">
        <option>JS</option>
        <option>modules</option>
        <option>are</option>
        <option>cool</option>
      </select>

      <cr-input></cr-input>

      <cr-icon-button iron-icon="cr:more-vert"></cr-icon-button>

      <div>
        <cr-button on-click="onClick_">Show toast</cr-button>
        <cr-toast><span>I am toasted</span></cr-toast>
      </div>

      <iron-icon icon="cr:error"></iron-icon>

      <div>
        <cr-radio-group id="radioGroup" selected="cr">
          <cr-radio-button name="cr">cr</cr-radio-button>
          <cr-radio-button name="radio">radio</cr-radio-button>
          <cr-radio-button name="buttons">buttons</cr-radio-button>
        </cr-radio-group>
      </div>

      <div>
        <cr-button on-click="showDrawer_">Show drawer</cr-button>
        <cr-drawer heading="Drawer">
          <div class="drawer-content">Content of drawer</div>
        </cr-drawer>
      </div>
    `;
  }

  static get properties() {
    return {
      /** @private */
      toggleChecked_: Boolean,

      /** @private */
      checkboxChecked_: Boolean,
    };
  }

  /** @private */
  onClick_() {
    this.shadowRoot.querySelector('cr-toast').show(2000);
  }

  /** @private */
  showDrawer_() {
    this.shadowRoot.querySelector('cr-drawer').openDrawer();
  }
}  // class HelloPolymer3

customElements.define('hello-polymer3', HelloPolymer3Element);
