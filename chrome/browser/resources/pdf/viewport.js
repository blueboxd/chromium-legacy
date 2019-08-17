// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Clamps the zoom factor (or page scale factor) to be within the limits.
 *
 * @param {number} factor The zoom/scale factor.
 * @return {number} The factor clamped within the limits.
 */
function clampZoom(factor) {
  return Math.max(
      Viewport.ZOOM_FACTOR_RANGE.min,
      Math.min(factor, Viewport.ZOOM_FACTOR_RANGE.max));
}

/**
 * Returns the area of the intersection of two rectangles.
 *
 * @param {!ViewportRect} rect1 the first rect
 * @param {!ViewportRect} rect2 the second rect
 * @return {number} the area of the intersection of the rects
 */
function getIntersectionArea(rect1, rect2) {
  const left = Math.max(rect1.x, rect2.x);
  const top = Math.max(rect1.y, rect2.y);
  const right = Math.min(rect1.x + rect1.width, rect2.x + rect2.width);
  const bottom = Math.min(rect1.y + rect1.height, rect2.y + rect2.height);

  if (left >= right || top >= bottom) {
    return 0;
  }

  return (right - left) * (bottom - top);
}

/**
 * Computes vector between two points.
 *
 * @param {!Point} p1 The first point.
 * @param {!Point} p2 The second point.
 * @return {!Point} The vector.
 */
function vectorDelta(p1, p2) {
  return {x: p2.x - p1.x, y: p2.y - p1.y};
}

function frameToPluginCoordinate(coordinateInFrame) {
  const container = $('plugin');
  return {
    x: coordinateInFrame.x - container.getBoundingClientRect().left,
    y: coordinateInFrame.y - container.getBoundingClientRect().top
  };
}

/** @implements {Viewport} */
class ViewportImpl {
  /**
   * Create a new viewport.
   *
   * @param {Window} window the window
   * @param {Object} sizer is the element which represents the size of the
   *     document in the viewport
   * @param {Function} viewportChangedCallback is run when the viewport changes
   * @param {Function} beforeZoomCallback is run before a change in zoom
   * @param {Function} afterZoomCallback is run after a change in zoom
   * @param {Function} setUserInitiatedCallback is run to indicate whether a
   *     zoom event is user initiated.
   * @param {number} scrollbarWidth the width of scrollbars on the page
   * @param {number} defaultZoom The default zoom level.
   * @param {number} topToolbarHeight The number of pixels that should initially
   *     be left blank above the document for the toolbar.
   */
  constructor(
      window, sizer, viewportChangedCallback, beforeZoomCallback,
      afterZoomCallback, setUserInitiatedCallback, scrollbarWidth, defaultZoom,
      topToolbarHeight) {
    this.window_ = window;
    this.sizer_ = sizer;
    this.viewportChangedCallback_ = viewportChangedCallback;
    this.beforeZoomCallback_ = beforeZoomCallback;
    this.afterZoomCallback_ = afterZoomCallback;
    this.setUserInitiatedCallback_ = setUserInitiatedCallback;
    this.allowedToChangeZoom_ = false;
    this.internalZoom_ = 1;
    this.zoomManager_ = new InactiveZoomManager(this, 1);
    /** @private {?DocumentDimensions} */
    this.documentDimensions_ = null;
    /** @private {Array<ViewportRect>} */
    this.pageDimensions_ = [];
    this.scrollbarWidth_ = scrollbarWidth;
    this.fittingType_ = FittingType.NONE;
    this.defaultZoom_ = defaultZoom;
    this.topToolbarHeight_ = topToolbarHeight;
    // |twoUpView_| should be in sync with |two_up_view_| in PDFiumEngine.
    this.twoUpView_ = false;
    this.prevScale_ = 1;
    this.pinchPhase_ = Viewport.PinchPhase.PINCH_NONE;
    this.pinchPanVector_ = null;
    this.pinchCenter_ = null;
    /** @private {?Point} */
    this.firstPinchCenterInFrame_ = null;
    this.rotations_ = 0;
    // TODO(dstockwell): why isn't this private?
    this.oldCenterInContent = null;
    this.keepContentCentered_ = null;

    window.addEventListener('scroll', this.updateViewport_.bind(this));
    window.addEventListener('resize', this.resizeWrapper_.bind(this));
  }

  /**
   * @param {number} n the number of clockwise 90-degree rotations to
   *     increment by.
   */
  rotateClockwise(n) {
    this.rotations_ = (this.rotations_ + n) % 4;
  }

  /**
   * @return {number} the number of clockwise 90-degree rotations that have been
   *     applied.
   */
  getClockwiseRotations() {
    return this.rotations_;
  }

  /**
   * Sets the state of two-up view.
   *
   * @param {boolean} twoUpView new two-up view state.
   */
  setTwoUpView(twoUpView) {
    this.twoUpView_ = twoUpView;
  }

  /**
   * Converts a page position (e.g. the location of a bookmark) to a screen
   * position.
   *
   * @param {number} page
   * @param {Point} point The position on `page`.
   * @return The screen position.
   */
  convertPageToScreen(page, point) {
    const dimensions = this.getPageInsetDimensions(page);

    // width & height are already rotated.
    const height = dimensions.height;
    const width = dimensions.width;

    const matrix = new DOMMatrix();

    const rotation = this.rotations_ * 90;
    // Set origin for rotation.
    if (rotation == 90) {
      matrix.translateSelf(width, 0);
    } else if (rotation == 180) {
      matrix.translateSelf(width, height);
    } else if (rotation == 270) {
      matrix.translateSelf(0, height);
    }
    matrix.rotateSelf(0, 0, rotation);

    // Invert Y position with respect to height as page coordinates are
    // measured from the bottom left.
    matrix.translateSelf(0, height);
    matrix.scaleSelf(1, -1);

    const pointsToPixels = 96 / 72;
    const result = matrix.transformPoint({
      x: point.x * pointsToPixels,
      y: point.y * pointsToPixels,
    });
    return {
      x: result.x + Viewport.PAGE_SHADOW.left,
      y: result.y + Viewport.PAGE_SHADOW.top,
    };
  }


  /**
   * Returns the zoomed and rounded document dimensions for the given zoom.
   * Rounding is necessary when interacting with the renderer which tends to
   * operate in integral values (for example for determining if scrollbars
   * should be shown).
   *
   * @param {number} zoom The zoom to use to compute the scaled dimensions.
   * @return {Object} A dictionary with scaled 'width'/'height' of the document.
   * @private
   */
  getZoomedDocumentDimensions_(zoom) {
    if (!this.documentDimensions_) {
      return null;
    }
    return {
      width: Math.round(this.documentDimensions_.width * zoom),
      height: Math.round(this.documentDimensions_.height * zoom)
    };
  }

  /** @override */
  getDocumentDimensions() {
    return {
      width: this.documentDimensions_.width,
      height: this.documentDimensions_.height
    };
  }

  /**
   * Returns the rectangle representing the viewport.
   *
   * @return {!ViewportRect} a ViewportRect containing x, y, width, and height
   *     of the viewport given current zoom.
   * @private
   */
  getViewportRect_() {
    return {
      x: this.position.x / this.zoom,
      y: this.position.y / this.zoom,
      width: this.size.width / this.zoom,
      height: this.size.height / this.zoom
    };
  }

  /**
   * @param {number} zoom compute whether scrollbars are needed at this zoom
   * @return {{horizontal: boolean, vertical: boolean}} whether horizontal or
   *     vertical scrollbars are needed.
   * @private
   */
  documentNeedsScrollbars_(zoom) {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(zoom);
    if (!zoomedDimensions) {
      return {horizontal: false, vertical: false};
    }

    // If scrollbars are required for one direction, expand the document in the
    // other direction to take the width of the scrollbars into account when
    // deciding whether the other direction needs scrollbars.
    if (zoomedDimensions.width > this.window_.innerWidth) {
      zoomedDimensions.height += this.scrollbarWidth_;
    } else if (zoomedDimensions.height > this.window_.innerHeight) {
      zoomedDimensions.width += this.scrollbarWidth_;
    }
    return {
      horizontal: zoomedDimensions.width > this.window_.innerWidth,
      vertical: zoomedDimensions.height + this.topToolbarHeight_ >
          this.window_.innerHeight
    };
  }

  /**
   * Returns true if the document needs scrollbars at the current zoom level.
   *
   * @return {Object} with 'x' and 'y' keys which map to bool values
   *     indicating if the horizontal and vertical scrollbars are needed
   *     respectively.
   */
  documentHasScrollbars() {
    return this.documentNeedsScrollbars_(this.zoom);
  }

  /**
   * Helper function called when the zoomed document size changes.
   *
   * @private
   */
  contentSizeChanged_() {
    const zoomedDimensions = this.getZoomedDocumentDimensions_(this.zoom);
    if (zoomedDimensions) {
      this.sizer_.style.width = zoomedDimensions.width + 'px';
      this.sizer_.style.height =
          zoomedDimensions.height + this.topToolbarHeight_ + 'px';
    }
  }

  /**
   * Called when the viewport should be updated.
   *
   * @private
   */
  updateViewport_() {
    this.viewportChangedCallback_();
  }

  /**
   * Called when the browser window size changes.
   *
   * @private
   */
  resizeWrapper_() {
    this.setUserInitiatedCallback_(false);
    this.resize_();
    this.setUserInitiatedCallback_(true);
  }

  /**
   * Called when the viewport size changes.
   *
   * @private
   */
  resize_() {
    if (this.fittingType_ == FittingType.FIT_TO_PAGE) {
      this.fitToPageInternal_(false);
    } else if (this.fittingType_ == FittingType.FIT_TO_WIDTH) {
      this.fitToWidth();
    } else if (this.fittingType_ == FittingType.FIT_TO_HEIGHT) {
      this.fitToHeightInternal_(false);
    } else if (this.internalZoom_ == 0) {
      this.fitToNone();
    } else {
      this.updateViewport_();
    }
  }

  /** @override */
  get position() {
    return {
      x: this.window_.pageXOffset,
      y: this.window_.pageYOffset - this.topToolbarHeight_
    };
  }

  /**
   * Scroll the viewport to the specified position.
   *
   * @param {Point} position The position to scroll to.
   */
  set position(position) {
    this.window_.scrollTo(position.x, position.y + this.topToolbarHeight_);
  }

  /** @override */
  get size() {
    const needsScrollbars = this.documentNeedsScrollbars_(this.zoom);
    const scrollbarWidth = needsScrollbars.vertical ? this.scrollbarWidth_ : 0;
    const scrollbarHeight =
        needsScrollbars.horizontal ? this.scrollbarWidth_ : 0;
    return {
      width: this.window_.innerWidth - scrollbarWidth,
      height: this.window_.innerHeight - scrollbarHeight
    };
  }

  /** @override */
  get zoom() {
    return this.zoomManager_.applyBrowserZoom(this.internalZoom_);
  }

  /**
   * Set the zoom manager.
   *
   * @type {ZoomManager} manager the zoom manager to set.
   */
  set zoomManager(manager) {
    this.zoomManager_ = manager;
  }

  /**
   * @return {Viewport.PinchPhase} The phase of the current pinch gesture for
   *    the viewport.
   */
  get pinchPhase() {
    return this.pinchPhase_;
  }

  /**
   * @return {Object} The panning caused by the current pinch gesture (as
   *    the deltas of the x and y coordinates).
   */
  get pinchPanVector() {
    return this.pinchPanVector_;
  }

  /**
   * @return {Object} The coordinates of the center of the current pinch
   *     gesture.
   */
  get pinchCenter() {
    return this.pinchCenter_;
  }

  /**
   * Used to wrap a function that might perform zooming on the viewport. This is
   * required so that we can notify the plugin that zooming is in progress
   * so that while zooming is taking place it can stop reacting to scroll events
   * from the viewport. This is to avoid flickering.
   *
   * @param {Function} f Function to wrap
   * @private
   */
  mightZoom_(f) {
    this.beforeZoomCallback_();
    this.allowedToChangeZoom_ = true;
    f();
    this.allowedToChangeZoom_ = false;
    this.afterZoomCallback_();
  }

  /**
   * Sets the zoom of the viewport.
   *
   * @param {number} newZoom the zoom level to zoom to.
   * @private
   */
  setZoomInternal_(newZoom) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    // Record the scroll position (relative to the top-left of the window).
    const currentScrollPos = {
      x: this.position.x / this.zoom,
      y: this.position.y / this.zoom
    };

    this.internalZoom_ = newZoom;
    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.position = {
      x: currentScrollPos.x * this.zoom,
      y: currentScrollPos.y * this.zoom
    };
  }

  /**
   * Sets the zoom of the viewport.
   * Same as setZoomInternal_ but for pinch zoom we have some more operations.
   *
   * @param {number} scaleDelta The zoom delta.
   * @param {!Object} center The pinch center in content coordinates.
   * @private
   */
  setPinchZoomInternal_(scaleDelta, center) {
    assert(
        this.allowedToChangeZoom_,
        'Called Viewport.setPinchZoomInternal_ without calling ' +
            'Viewport.mightZoom_.');
    this.internalZoom_ = clampZoom(this.internalZoom_ * scaleDelta);

    const newCenterInContent = this.frameToContent(center);
    const delta = {
      x: (newCenterInContent.x - this.oldCenterInContent.x),
      y: (newCenterInContent.y - this.oldCenterInContent.y)
    };

    // Record the scroll position (relative to the pinch center).
    const currentScrollPos = {
      x: this.position.x - delta.x * this.zoom,
      y: this.position.y - delta.y * this.zoom
    };

    this.contentSizeChanged_();
    // Scroll to the scaled scroll position.
    this.position = {x: currentScrollPos.x, y: currentScrollPos.y};
  }

  /**
   *  Converts a point from frame to content coordinates.
   *
   *  @param {!Object} framePoint The frame coordinates.
   *  @return {!Object} The content coordinates.
   *  @private
   */
  frameToContent(framePoint) {
    // TODO(mcnee) Add a helper Point class to avoid duplicating operations
    // on plain {x,y} objects.
    return {
      x: (framePoint.x + this.position.x) / this.zoom,
      y: (framePoint.y + this.position.y) / this.zoom
    };
  }

  /**
   * Sets the zoom to the given zoom level.
   *
   * @param {number} newZoom the zoom level to zoom to.
   */
  setZoom(newZoom) {
    this.fittingType_ = FittingType.NONE;
    this.mightZoom_(() => {
      this.setZoomInternal_(clampZoom(newZoom));
      this.updateViewport_();
    });
  }

  /** @override */
  updateZoomFromBrowserChange(oldBrowserZoom) {
    this.mightZoom_(() => {
      // Record the scroll position (relative to the top-left of the window).
      const oldZoom = oldBrowserZoom * this.internalZoom_;
      const currentScrollPos = {
        x: this.position.x / oldZoom,
        y: this.position.y / oldZoom
      };
      this.contentSizeChanged_();
      // Scroll to the scaled scroll position.
      this.position = {
        x: currentScrollPos.x * this.zoom,
        y: currentScrollPos.y * this.zoom
      };
      this.updateViewport_();
    });
  }

  /**
   * @return {number} the width of scrollbars in the viewport in pixels.
   */
  get scrollbarWidth() {
    return this.scrollbarWidth_;
  }

  /**
   * @return {FittingType} the fitting type the viewport is currently in.
   */
  get fittingType() {
    return this.fittingType_;
  }

  /**
   * Get the page at a given y position. If there are multiple pages
   * overlapping the given y-coordinate, return the page with the smallest
   * index.
   *
   * @param {number} y the y-coordinate to get the page at.
   * @return {number} the index of a page overlapping the given y-coordinate.
   * @private
   */
  getPageAtY_(y) {
    let min = 0;
    let max = this.pageDimensions_.length - 1;
    while (max >= min) {
      const page = Math.floor(min + ((max - min) / 2));
      // There might be a gap between the pages, in which case use the bottom
      // of the previous page as the top for finding the page.
      let top = 0;
      if (page > 0) {
        top = this.pageDimensions_[page - 1].y +
            this.pageDimensions_[page - 1].height;
      }
      const bottom =
          this.pageDimensions_[page].y + this.pageDimensions_[page].height;

      if (top <= y && bottom > y) {
        return page;
      }

      if (top > y) {
        max = page - 1;
      } else {
        min = page + 1;
      }
    }
    return 0;
  }

  /**
   * Return the last page visible in the viewport. Returns the last index of the
   * document if the viewport is below the document.
   *
   * @return {number} the highest index of the pages visible in the viewport.
   * @private
   */
  getLastPageInViewport_() {
    const viewportRect = this.getViewportRect_();
    const pageAtY = this.getPageAtY_(viewportRect.y + viewportRect.height);

    if (!this.twoUpView_ || pageAtY % 2 == 1 ||
        pageAtY + 1 >= this.pageDimensions_.length) {
      return pageAtY;
    }

    const nextPage = this.pageDimensions_[pageAtY + 1];
    return getIntersectionArea(viewportRect, nextPage) > 0 ? pageAtY + 1 :
                                                             pageAtY;
  }

  /** @override */
  isPointInsidePage(point) {
    const zoom = this.zoom;
    const size = this.size;
    const position = this.position;
    const page = this.getPageAtY_((position.y + point.y) / zoom);
    const pageWidth = this.pageDimensions_[page].width * zoom;
    const documentWidth = this.getDocumentDimensions().width * zoom;

    const outerWidth = Math.max(size.width, documentWidth);

    if (pageWidth >= outerWidth) {
      return true;
    }

    const x = point.x + position.x;

    const minX = (outerWidth - pageWidth) / 2;
    const maxX = outerWidth - minX;
    return x >= minX && x <= maxX;
  }

  /**
   * Returns the page with the greatest proportion of its area in the current
   * viewport.
   *
   * @return {number} the index of the most visible page.
   */
  getMostVisiblePage() {
    const viewportRect = this.getViewportRect_();

    const firstVisiblePage = this.getPageAtY_(viewportRect.y);
    const lastPossibleVisiblePage = this.getLastPageInViewport_();
    if (firstVisiblePage === lastPossibleVisiblePage) {
      return firstVisiblePage;
    }

    let mostVisiblePage = firstVisiblePage;
    let largestIntersection = 0;

    for (let i = firstVisiblePage; i < lastPossibleVisiblePage + 1; i++) {
      const pageArea =
          this.pageDimensions_[i].width * this.pageDimensions_[i].height;

      // TODO(thestig): check whether we can remove this check.
      if (pageArea <= 0) {
        continue;
      }

      const pageIntersectionArea =
          getIntersectionArea(this.pageDimensions_[i], viewportRect) / pageArea;

      if (pageIntersectionArea > largestIntersection) {
        mostVisiblePage = i;
        largestIntersection = pageIntersectionArea;
      }
    }

    return mostVisiblePage;
  }

  /**
   * Compute the zoom level for fit-to-page, fit-to-width or fit-to-height.
   *
   * At least one of {fitWidth, fitHeight} must be true.
   *
   * @param {Object} pageDimensions the dimensions of a given page in px.
   * @param {boolean} fitWidth a bool indicating whether the whole width of the
   *     page needs to be in the viewport.
   * @param {boolean} fitHeight a bool indicating whether the whole height of
   *     the page needs to be in the viewport.
   * @return {number} the internal zoom to set
   * @private
   */
  computeFittingZoom_(pageDimensions, fitWidth, fitHeight) {
    assert(
        fitWidth || fitHeight,
        'Invalid parameters. At least one of fitWidth and fitHeight must be ' +
            'true.');

    // First compute the zoom without scrollbars.
    let zoom = this.computeFittingZoomGivenDimensions_(
        fitWidth, fitHeight, this.window_.innerWidth, this.window_.innerHeight,
        pageDimensions.width, pageDimensions.height);

    // Check if there needs to be any scrollbars.
    const needsScrollbars = this.documentNeedsScrollbars_(zoom);

    // If the document fits, just return the zoom.
    if (!needsScrollbars.horizontal && !needsScrollbars.vertical) {
      return zoom;
    }

    const zoomedDimensions = this.getZoomedDocumentDimensions_(zoom);

    // Check if adding a scrollbar will result in needing the other scrollbar.
    const scrollbarWidth = this.scrollbarWidth_;
    if (needsScrollbars.horizontal &&
        zoomedDimensions.height > this.window_.innerHeight - scrollbarWidth) {
      needsScrollbars.vertical = true;
    }
    if (needsScrollbars.vertical &&
        zoomedDimensions.width > this.window_.innerWidth - scrollbarWidth) {
      needsScrollbars.horizontal = true;
    }

    // Compute available window space.
    const windowWithScrollbars = {
      width: this.window_.innerWidth,
      height: this.window_.innerHeight
    };
    if (needsScrollbars.horizontal) {
      windowWithScrollbars.height -= scrollbarWidth;
    }
    if (needsScrollbars.vertical) {
      windowWithScrollbars.width -= scrollbarWidth;
    }

    // Recompute the zoom.
    zoom = this.computeFittingZoomGivenDimensions_(
        fitWidth, fitHeight, windowWithScrollbars.width,
        windowWithScrollbars.height, pageDimensions.width,
        pageDimensions.height);

    return this.zoomManager_.internalZoomComponent(zoom);
  }

  /**
   * Compute a zoom level given the dimensions to fit and the actual numbers
   * in those dimensions.
   *
   * @param {boolean} fitWidth make sure the page width is totally contained in
   *     the window.
   * @param {boolean} fitHeight make sure the page height is totally contained
   *     in the window.
   * @param {number} windowWidth the width of the window in px.
   * @param {number} windowHeight the height of the window in px.
   * @param {number} pageWidth the width of the page in px.
   * @param {number} pageHeight the height of the page in px.
   * @return {number} the internal zoom to set
   * @private
   */
  computeFittingZoomGivenDimensions_(
      fitWidth, fitHeight, windowWidth, windowHeight, pageWidth, pageHeight) {
    // Assumes at least one of {fitWidth, fitHeight} is set.
    let zoomWidth;
    let zoomHeight;

    if (fitWidth) {
      zoomWidth = windowWidth / pageWidth;
    }

    if (fitHeight) {
      zoomHeight = windowHeight / pageHeight;
    }

    let zoom;
    if (!fitWidth && fitHeight) {
      zoom = zoomHeight;
    } else if (fitWidth && !fitHeight) {
      zoom = zoomWidth;
    } else {
      // Assume fitWidth && fitHeight
      zoom = Math.min(zoomWidth, zoomHeight);
    }

    return Math.max(zoom, 0);
  }

  /**
   * Zoom the viewport so that the page width consumes the entire viewport.
   */
  fitToWidth() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_WIDTH;
      if (!this.documentDimensions_) {
        return;
      }
      // When computing fit-to-width, the maximum width of a page in the
      // document is used, which is equal to the size of the document width.
      this.setZoomInternal_(
          this.computeFittingZoom_(this.documentDimensions_, true, false));
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that the page height consumes the entire viewport.
   *
   * @param {boolean} scrollToTopOfPage Set to true if the viewport should be
   *     scrolled to the top of the current page. Set to false if the viewport
   *     should remain at the current scroll position.
   * @private
   */
  fitToHeightInternal_(scrollToTopOfPage) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_HEIGHT;
      if (!this.documentDimensions_) {
        return;
      }
      const page = this.getMostVisiblePage();
      // When computing fit-to-height, the maximum height of the current page
      // is used.
      const dimensions = {
        width: 0,
        height: this.pageDimensions_[page].height,
      };
      this.setZoomInternal_(this.computeFittingZoom_(dimensions, false, true));
      if (scrollToTopOfPage) {
        this.position = {x: 0, y: this.pageDimensions_[page].y * this.zoom};
      }
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that the page height consumes the entire viewport.
   */
  fitToHeight() {
    this.fitToHeightInternal_(true);
  }

  /**
   * Zoom the viewport so that a page consumes as much as possible of the it.
   *
   * @param {boolean} scrollToTopOfPage Set to true if the viewport should be
   *     scrolled to the top of the current page. Set to false if the viewport
   *     should remain at the current scroll position.
   * @private
   */
  fitToPageInternal_(scrollToTopOfPage) {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.FIT_TO_PAGE;
      if (!this.documentDimensions_) {
        return;
      }
      const page = this.getMostVisiblePage();
      // Fit to the current page's height and the widest page's width.
      const dimensions = {
        width: this.documentDimensions_.width,
        height: this.pageDimensions_[page].height,
      };
      this.setZoomInternal_(this.computeFittingZoom_(dimensions, true, true));
      if (scrollToTopOfPage) {
        this.position = {x: 0, y: this.pageDimensions_[page].y * this.zoom};
      }
      this.updateViewport_();
    });
  }

  /**
   * Zoom the viewport so that a page consumes the entire viewport. Also scrolls
   * the viewport to the top of the current page.
   */
  fitToPage() {
    this.fitToPageInternal_(true);
  }

  /**
   * Zoom the viewport to the default zoom policy.
   */
  fitToNone() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      if (!this.documentDimensions_) {
        return;
      }
      this.setZoomInternal_(Math.min(
          this.defaultZoom_,
          this.computeFittingZoom_(this.documentDimensions_, true, false)));
      this.updateViewport_();
    });
  }

  /**
   * Zoom out to the next predefined zoom level.
   */
  zoomOut() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      let nextZoom = Viewport.ZOOM_FACTORS[0];
      for (let i = 0; i < Viewport.ZOOM_FACTORS.length; i++) {
        if (Viewport.ZOOM_FACTORS[i] < this.internalZoom_) {
          nextZoom = Viewport.ZOOM_FACTORS[i];
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
    });
  }

  /**
   * Zoom in to the next predefined zoom level.
   */
  zoomIn() {
    this.mightZoom_(() => {
      this.fittingType_ = FittingType.NONE;
      let nextZoom = Viewport.ZOOM_FACTORS[Viewport.ZOOM_FACTORS.length - 1];
      for (let i = Viewport.ZOOM_FACTORS.length - 1; i >= 0; i--) {
        if (Viewport.ZOOM_FACTORS[i] > this.internalZoom_) {
          nextZoom = Viewport.ZOOM_FACTORS[i];
        }
      }
      this.setZoomInternal_(nextZoom);
      this.updateViewport_();
    });
  }

  /**
   * Pinch zoom event handler.
   *
   * @param {!Object} e The pinch event.
   */
  pinchZoom(e) {
    this.mightZoom_(() => {
      this.pinchPhase_ = e.direction == 'out' ?
          Viewport.PinchPhase.PINCH_UPDATE_ZOOM_OUT :
          Viewport.PinchPhase.PINCH_UPDATE_ZOOM_IN;

      const scaleDelta = e.startScaleRatio / this.prevScale_;
      if (this.firstPinchCenterInFrame_ != null) {
        this.pinchPanVector_ =
            vectorDelta(e.center, this.firstPinchCenterInFrame_);
      }

      const needsScrollbars =
          this.documentNeedsScrollbars_(this.zoomManager_.applyBrowserZoom(
              clampZoom(this.internalZoom_ * scaleDelta)));

      this.pinchCenter_ = e.center;

      // If there's no horizontal scrolling, keep the content centered so the
      // user can't zoom in on the non-content area.
      // TODO(mcnee) Investigate other ways of scaling when we don't have
      // horizontal scrolling. We want to keep the document centered,
      // but this causes a potentially awkward transition when we start
      // using the gesture center.
      if (!needsScrollbars.horizontal) {
        this.pinchCenter_ = {
          x: this.window_.innerWidth / 2,
          y: this.window_.innerHeight / 2
        };
      } else if (this.keepContentCentered_) {
        this.oldCenterInContent =
            this.frameToContent(frameToPluginCoordinate(e.center));
        this.keepContentCentered_ = false;
      }

      this.setPinchZoomInternal_(scaleDelta, frameToPluginCoordinate(e.center));
      this.updateViewport_();
      this.prevScale_ = e.startScaleRatio;
    });
  }

  /** @param {!Object} e The pinch event. */
  pinchZoomStart(e) {
    this.pinchPhase_ = Viewport.PinchPhase.PINCH_START;
    this.prevScale_ = 1;
    this.oldCenterInContent =
        this.frameToContent(frameToPluginCoordinate(e.center));

    const needsScrollbars = this.documentNeedsScrollbars_(this.zoom);
    this.keepContentCentered_ = !needsScrollbars.horizontal;
    // We keep track of begining of the pinch.
    // By doing so we will be able to compute the pan distance.
    this.firstPinchCenterInFrame_ = e.center;
  }

  /** @param {!Object} e The pinch event. */
  pinchZoomEnd(e) {
    this.mightZoom_(() => {
      this.pinchPhase_ = Viewport.PinchPhase.PINCH_END;
      const scaleDelta = e.startScaleRatio / this.prevScale_;
      this.pinchCenter_ = e.center;

      this.setPinchZoomInternal_(scaleDelta, frameToPluginCoordinate(e.center));
      this.updateViewport_();
    });

    this.pinchPhase_ = Viewport.PinchPhase.PINCH_NONE;
    this.pinchPanVector_ = null;
    this.pinchCenter_ = null;
    this.firstPinchCenterInFrame_ = null;
  }

  /**
   * Go to the next page. If the document is in two-up view, go to the left page
   * of the next row.
   */
  goToNextPage() {
    const currentPage = this.getMostVisiblePage();
    const nextPageOffset = (this.twoUpView_ && currentPage % 2 == 0) ? 2 : 1;
    this.goToPage(currentPage + nextPageOffset);
  }

  /**
   * Go to the previous page. If the document is in two-up view, go to the left
   * page of the previous row.
   */
  goToPreviousPage() {
    const currentPage = this.getMostVisiblePage();
    let previousPageOffset = -1;

    if (this.twoUpView_) {
      previousPageOffset = (currentPage % 2 == 0) ? -2 : -3;
    }

    this.goToPage(currentPage + previousPageOffset);
  }

  /**
   * Go to the given page index.
   *
   * @param {number} page the index of the page to go to. zero-based.
   */
  goToPage(page) {
    this.goToPageAndXY(page, 0, 0);
  }

  /**
   * Go to the given y position in the given page index.
   *
   * @param {number} page the index of the page to go to. zero-based.
   * @param {number} x the x position in the page to go to.
   * @param {number} y the y position in the page to go to.
   */
  goToPageAndXY(page, x, y) {
    this.mightZoom_(() => {
      if (this.pageDimensions_.length === 0) {
        return;
      }
      if (page < 0) {
        page = 0;
      }
      if (page >= this.pageDimensions_.length) {
        page = this.pageDimensions_.length - 1;
      }
      const dimensions = this.pageDimensions_[page];
      let toolbarOffset = 0;
      // Unless we're in fit to page or fit to height mode, scroll above the
      // page by |this.topToolbarHeight_| so that the toolbar isn't covering it
      // initially.
      if (!this.isPagedMode()) {
        toolbarOffset = this.topToolbarHeight_;
      }
      this.position = {
        x: (dimensions.x + x) * this.zoom,
        y: (dimensions.y + y) * this.zoom - toolbarOffset
      };
      this.updateViewport_();
    });
  }

  /**
   * Set the dimensions of the document.
   *
   * @param {DocumentDimensions} documentDimensions the dimensions of the
   *     document
   */
  setDocumentDimensions(documentDimensions) {
    this.mightZoom_(() => {
      const initialDimensions = !this.documentDimensions_;
      this.documentDimensions_ = documentDimensions;
      this.pageDimensions_ = this.documentDimensions_.pageDimensions;
      if (initialDimensions) {
        this.setZoomInternal_(Math.min(
            this.defaultZoom_,
            this.computeFittingZoom_(this.documentDimensions_, true, false)));
        this.position = {x: 0, y: -this.topToolbarHeight_};
      }
      this.contentSizeChanged_();
      this.resize_();
    });
  }

  /**
   * @param {number} page
   * @return {ViewportRect} The bounds for page `page` minus the shadows.
   */
  getPageInsetDimensions(page) {
    const pageDimensions = this.pageDimensions_[page];
    const shadow = Viewport.PAGE_SHADOW;
    return {
      x: pageDimensions.x + shadow.left,
      y: pageDimensions.y + shadow.top,
      width: pageDimensions.width - shadow.left - shadow.right,
      height: pageDimensions.height - shadow.top - shadow.bottom,
    };
  }

  /**
   * Get the coordinates of the page contents (excluding the page shadow)
   * relative to the screen.
   *
   * @param {number} page the index of the page to get the rect for.
   * @return {Object} a rect representing the page in screen coordinates.
   */
  getPageScreenRect(page) {
    if (!this.documentDimensions_) {
      return {x: 0, y: 0, width: 0, height: 0};
    }
    if (page >= this.pageDimensions_.length) {
      page = this.pageDimensions_.length - 1;
    }

    const pageDimensions = this.pageDimensions_[page];

    // Compute the page dimensions minus the shadows.
    const insetDimensions = this.getPageInsetDimensions(page);

    // Compute the x-coordinate of the page within the document.
    // TODO(raymes): This should really be set when the PDF plugin passes the
    // page coordinates, but it isn't yet.
    const x = (this.documentDimensions_.width - pageDimensions.width) / 2 +
        Viewport.PAGE_SHADOW.left;
    // Compute the space on the left of the document if the document fits
    // completely in the screen.
    let spaceOnLeft =
        (this.size.width - this.documentDimensions_.width * this.zoom) / 2;
    spaceOnLeft = Math.max(spaceOnLeft, 0);

    return {
      x: x * this.zoom + spaceOnLeft - this.window_.pageXOffset,
      y: insetDimensions.y * this.zoom - this.window_.pageYOffset,
      width: insetDimensions.width * this.zoom,
      height: insetDimensions.height * this.zoom
    };
  }

  /**
   * Check if the current fitting type is a paged mode.
   *
   * In a paged mode, page up and page down scroll to the top of the
   * previous/next page and part of the page is under the toolbar.
   *
   * @return {boolean} Whether the current fitting type is a paged mode.
   */
  isPagedMode() {
    return (
        this.fittingType_ == FittingType.FIT_TO_PAGE ||
        this.fittingType_ == FittingType.FIT_TO_HEIGHT);
  }

  /**
   * Scroll the viewport to the specified position.
   *
   * @param {!Point} point The position to which to move the viewport.
   */
  scrollTo(point) {
    let changed = false;
    const newPosition = this.position;
    if (point.x !== undefined && point.x != newPosition.x) {
      newPosition.x = point.x;
      changed = true;
    }
    if (point.y !== undefined && point.y != newPosition.y) {
      newPosition.y = point.y;
      changed = true;
    }

    if (changed) {
      this.position = newPosition;
    }
  }

  /**
   * Scroll the viewport by the specified delta.
   *
   * @param {!Point} delta The delta by which to move the viewport.
   */
  scrollBy(delta) {
    const newPosition = this.position;
    newPosition.x += delta.x;
    newPosition.y += delta.y;
    this.scrollTo(newPosition);
  }
}
