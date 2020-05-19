// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-entry' is an element that displays
 * one certificate provisioning processes.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificateProvisioningActionEventDetail, CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import {CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

// <if expr="chromeos">
// TODO(https://crbug.com/1071641): When it is possible to have conditional
// imports in ui/webui/resources/cr_components/, this file should be
// conditionally imported. Until then, it is imported unconditionally but its
// contents are omitted for non-ChromeOS platforms.

Polymer({
  is: 'certificate-provisioning-entry',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!CertificateProvisioningProcess} */
    model: Object,
  },

  behaviors: [I18nBehavior],

  /** @private */
  closePopupMenu_() {
    this.$$('cr-action-menu').close();
  },

  /** @private */
  onDotsClick_() {
    const actionMenu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    actionMenu.showAt(this.$.dots);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDetailsClick_(event) {
    this.closePopupMenu_();
    this.fire(
        CertificateProvisioningViewDetailsActionEvent,
        /** @type {!CertificateProvisioningActionEventDetail} */ ({
          model: this.model,
          anchor: this.$.dots,
        }));
  },
});

// </if>
