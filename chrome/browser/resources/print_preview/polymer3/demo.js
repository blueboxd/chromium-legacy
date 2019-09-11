// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class HelloPolymer3Element extends PolymerElement {
  static get template() {
    return html`
      <style include="md-select">
        cr-toggle {
          display: inline-block;
        }

        cr-icon-button {
          --cr-icon-button-color: white;
        }

        .setting {
          align-items: center;
          display: flex;
        }

        div, cr-input, select, cr-checkbox {
          margin-top: 20px;
        }
      </style>

      <cr-toolbar id="toolbar" page-name="Polymer 3 Demo"
          search-prompt="Search">
        <cr-icon-button iron-icon="cr:more-vert"></cr-icon-button>
      </cr-toolbar>

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

      <div>
        <cr-tabs selected="{{selectedSubpage_}}" tab-names="[[tabNames_]]">
        </cr-tabs>
        <div>
          <template is="dom-if" if="[[isTabASelected_(selectedSubpage_)]]">
            <span>This is Tab A</span>
          </template>
          <template is="dom-if" if="[[isTabBSelected_(selectedSubpage_)]]">
            <span>This is Tab B</span>
          </template>
        </div>
      </div>

      <div>
        <cr-button on-click="showDialog_">Click to open dialog</cr-button>
        <cr-dialog id="dialog">
          <div slot="title">I am a dialog</div>
        </cr-dialog>
      </div>

      <div>
        <cr-search-field label="test search field"></cr-search-field>
      </div>

      <div>
        <cr-expand-button on-click="onExpand_">Expand</cr-expand-button>
        <div hidden$="[[!expanded_]]">Expanded content</div>
      </div>

      <div class="setting">
        <span>Some setting</span>
        <cr-tooltip-icon tooltip-text="This setting is controlled by policy"
            icon-class="cr20:domain"
            icon-aria-label="This setting is controlled by policy">
        </cr-tooltip-icon>
        <cr-toggle disabled checked></cr-toggle>
      </div>

      <div>
        <cr-link-row class="hr" label="Hello Link Row"></cr-link-row>
      </div>
    `;
  }

  static get properties() {
    return {
      /** @private */
      toggleChecked_: Boolean,

      /** @private */
      checkboxChecked_: Boolean,

      /** @private */
      expanded_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      selectedSubpage_: {
        type: Number,
        value: 0,
      },

      /** @private {Array<string>} */
      tabNames_: {
        type: Array,
        value: () => (['A', 'B']),
      },
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

  /** @private */
  showDialog_() {
    this.shadowRoot.querySelector('cr-dialog').showModal();
  }

  /**
   * @return {boolean}
   * @private
   */
  isTabASelected_() {
    return this.selectedSubpage_ === 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  isTabBSelected_() {
    return this.selectedSubpage_ === 1;
  }

  /** @private */
  onExpand_() {
    this.expanded_ = !this.expanded_;
  }
}  // class HelloPolymer3

customElements.define('hello-polymer3', HelloPolymer3Element);
