// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * settings-idle-load is a simple variant of dom-if designed for lazy
 * loading and rendering of elements that are accessed imperatively. A URL is
 * given that holds the elements to be loaded lazily.
 */
Polymer({
  is: 'settings-idle-load',

  properties: {
    /**
     * If specified, it will be loaded via an HTML import before stamping the
     * template.
     */
    url: String,
  },

  /** @private {?Element} */
  child_: null,

  /** @private {?Element} */
  instance_: null,

  /** @private {number} */
  idleCallback_: 0,

  /** @override */
  attached() {
    this.idleCallback_ = requestIdleCallback(() => {
      this.get();
    });
  },

  /** @override */
  detached() {
    // No-op if callback already fired.
    cancelIdleCallback(this.idleCallback_);
  },

  /**
   * @return {!Promise<Element>} Child element which has been stamped into the
   *     DOM tree.
   */
  get() {
    if (this.loading_) {
      return this.loading_;
    }

    this.loading_ = new Promise((resolve, reject) => {
      this.importHref(this.url, () => {
        const template =
            /** @type {!HTMLTemplateElement} */ (this.getContentChildren()[0]);
        const TemplateClass = Polymer.Templatize.templatize(template, this, {
          mutableData: false,
          forwardHostProp: this._forwardHostPropV2,
        });

        this.instance_ = new TemplateClass();

        assert(!this.child_);
        this.child_ = this.instance_.root.firstElementChild;

        this.parentNode.insertBefore(this.instance_.root, this);
        resolve(this.child_);

        this.fire('lazy-loaded');
      }, reject, true);
    });

    return this.loading_;
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
