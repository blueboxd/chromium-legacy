// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
Polymer({
  is: 'viewer-pdf-toolbar',

  properties: {
    /**
     * The current loading progress of the PDF document (0 - 100).
     */
    loadProgress: {type: Number, observer: 'loadProgressChanged_'},

    /**
     * The title of the PDF document.
     */
    docTitle: String,

    /**
     * The number of the page being viewed (1-based).
     */
    pageNo: Number,

    /**
     * Tree of PDF bookmarks (or null if the document has no bookmarks).
     */
    bookmarks: {type: Object, value: null},

    /**
     * The number of pages in the PDF document.
     */
    docLength: Number,

    /**
     * Whether the toolbar is opened and visible.
     */
    opened: {type: Boolean, value: true},

    /**
     * Whether the viewer is currently in annotation mode.
     */
    annotationMode: {
      type: Boolean,
      notify: true,
      value: false,
      reflectToAttribute: true,
    },

    annotationTool: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * Whether annotation mode can be entered. This would be false if for
     * example the PDF is encrypted or password protected. Note, this is
     * true regardless of whether the feature flag is enabled.
     */
    annotationAvailable: {
      type: Boolean,
      value: true,
    },

    canUndoAnnotation: {
      type: Boolean,
      value: false,
    },

    canRedoAnnotation: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the PDF Annotations feature is enabled.
     */
    pdfAnnotationsEnabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the Printing feature is enabled.
     */
    printingEnabled: {
      type: Boolean,
      value: false,
    },

    strings: Object,
  },

  /**
   * @param {number} newProgress
   * @param {number} oldProgress
   * @private
   */
  loadProgressChanged_: function(newProgress, oldProgress) {
    const loaded = newProgress >= 100;
    const progressReset = newProgress < oldProgress;
    if (progressReset || loaded) {
      this.$.pageselector.classList.toggle('invisible', !loaded);
      this.$.buttons.classList.toggle('invisible', !loaded);
      this.$.progress.style.opacity = loaded ? 0 : 1;
      this.$['annotations-bar'].hidden = !loaded || !this.annotationMode;
    }
  },

  hide: function() {
    if (this.opened && !this.shouldKeepOpen()) {
      this.toggleVisibility();
    }
  },

  show: function() {
    if (!this.opened) {
      this.toggleVisibility();
    }
  },

  toggleVisibility: function() {
    this.opened = !this.opened;

    // We keep a handle on the animation in order to cancel the filling
    // behavior of previous animations.
    if (this.animation_) {
      this.animation_.cancel();
    }

    if (this.opened) {
      this.animation_ = this.animate(
          {
            transform: ['translateY(-100%)', 'translateY(0%)'],
          },
          {
            easing: 'cubic-bezier(0, 0, 0.2, 1)',
            duration: 250,
            fill: 'forwards',
          });
    } else {
      this.animation_ = this.animate(
          {
            transform: ['translateY(0%)', 'translateY(-100%)'],
          },
          {
            easing: 'cubic-bezier(0.4, 0, 1, 1)',
            duration: 250,
            fill: 'forwards',
          });
    }
  },

  selectPageNumber: function() {
    this.$.pageselector.select();
  },

  shouldKeepOpen: function() {
    return this.$.bookmarks.dropdownOpen || this.loadProgress < 100 ||
        this.$.pageselector.isActive() || this.annotationMode;
  },

  hideDropdowns: function() {
    let result = false;
    if (this.$.bookmarks.dropdownOpen) {
      this.$.bookmarks.toggleDropdown();
      result = true;
    }
    if (this.$.pen.dropdownOpen) {
      this.$.pen.toggleDropdown();
      result = true;
    }
    if (this.$.highlighter.dropdownOpen) {
      this.$.highlighter.toggleDropdown();
      result = true;
    }
    return result;
  },

  setDropdownLowerBound: function(lowerBound) {
    this.$.bookmarks.lowerBound = lowerBound;
  },

  rotateRight: function() {
    this.fire('rotate-right');
  },

  download: function() {
    this.fire('save');
  },

  print: function() {
    this.fire('print');
  },

  undo: function() {
    this.fire('undo');
  },

  redo: function() {
    this.fire('redo');
  },

  toggleAnnotation: function() {
    this.annotationMode = !this.annotationMode;
    if (this.annotationMode) {
      // Select pen tool when entering annotation mode.
      this.updateAnnotationTool_(this.$.pen);
    }
    this.dispatchEvent(new CustomEvent('annotation-mode-toggled', {
      detail: {
        value: this.annotationMode,
      },
    }));
  },

  /** @param {Event} e */
  annotationToolClicked_: function(e) {
    this.updateAnnotationTool_(e.currentTarget);
  },

  /** @param {Event} e */
  annotationToolOptionChanged_: function(e) {
    const element = e.currentTarget.parentElement;
    if (!this.annotationTool || element.id != this.annotationTool.tool) {
      return;
    }
    this.updateAnnotationTool_(e.currentTarget.parentElement);
  },

  /** @param {Element} element */
  updateAnnotationTool_: function(element) {
    const tool = element.id;
    const options = element.querySelector('viewer-pen-options') || {
      selectedSize: 1,
      selectedColor: null,
    };
    element.attributeStyleMap.set('--pen-tip-fill', options.selectedColor);
    element.attributeStyleMap.set(
        '--pen-tip-border',
        options.selectedColor == '#000000' ? 'currentcolor' :
                                             options.selectedColor);
    this.annotationTool = {
      tool: tool,
      size: options.selectedSize,
      color: options.selectedColor,
    };
  },

  /**
   * Used to determine equality in computed bindings.
   *
   * @param {*} a
   * @param {*} b
   */
  equal_: function(a, b) {
    return a == b;
  },

});
})();
