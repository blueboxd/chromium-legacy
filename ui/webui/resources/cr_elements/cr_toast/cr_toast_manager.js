// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.toastManager', () => {
  /* eslint-disable */
  /** @private {?cr.toastManager.CrToastManagerElement} */
  let toastManagerInstance = null;
  /* eslint-enable */

  /** @return {!cr.toastManager.CrToastManagerElement} */
  /* #export */ function getToastManager() {
    return assert(toastManagerInstance);
  }

  /** @param {?cr.toastManager.CrToastManagerElement} instance */
  function setInstance(instance) {
    assert(!instance || !toastManagerInstance);
    toastManagerInstance = instance;
  }

  /**
   * @fileoverview Element which shows toasts with optional undo button.
   */
  // eslint-disable-next-line
  /* #export */ let CrToastManagerElement = Polymer({
    is: 'cr-toast-manager',

    properties: {
      duration: {
        type: Number,
        value: 0,
      },
    },

    /** @return {boolean} */
    get isToastOpen() {
      return this.$.toast.open;
    },

    /** @override */
    attached: function() {
      setInstance(this);
    },

    /** @override */
    detached: function() {
      setInstance(null);
    },

    /** @param {string} label The label to display inside the toast. */
    show: function(label) {
      this.$.content.textContent = label;
      this.showInternal_();
    },

    /**
     * Shows the toast, making certain text fragments collapsible.
     * @param {!Array<!{value: string, collapsible: boolean}>} pieces
     */
    showForStringPieces: function(pieces) {
      const content = this.$.content;
      content.textContent = '';
      pieces.forEach(function(p) {
        if (p.value.length == 0) {
          return;
        }

        const span = document.createElement('span');
        span.textContent = p.value;
        if (p.collapsible) {
          span.classList.add('collapsible');
        }

        content.appendChild(span);
      });

      this.showInternal_();
    },

    /** @private */
    showInternal_: function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {
        text: this.$.content.textContent,
      });
      this.$.toast.show();
    },

    hide: function() {
      this.$.toast.hide();
    },
  });

  // #cr_define_end
  return {
    CrToastManagerElement: CrToastManagerElement,
    getToastManager: getToastManager,
  };
});
