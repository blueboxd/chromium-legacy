// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI.
 */

// Namespace of SVG elements
var svgNS = 'http://www.w3.org/2000/svg';

// Background color for the band with events.
var bandColor = '#d3d3d3';

// Color that should never appear on UI.
var unusedColor = '#ff0000';

// Supported zooms, mcs per pixel
var zooms = [2.5, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0];

// Active zoom level, as index in |zooms|. By default 100 mcs per pixel.
var zoomLevel = 5;

/**
 * Keep in sync with ArcTracingGraphicsModel::BufferEventType
 * See chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h.
 * Describes how events should be rendered. |color| specifies color of the
 * event, |name| is used in tooltips. |width| defines the width in case it is
 * rendered as a line and |radius| defines the radius in case it is rendered as
 * a circle.
 */
var eventAttributes = {
  // kIdleIn
  0: {color: bandColor, name: 'idle'},
  // kIdleOut
  1: {color: '#ffbf00', name: 'active'},

  // kBufferQueueDequeueStart
  100: {color: '#99cc00', name: 'app requests buffer'},
  // kBufferQueueDequeueDone
  101: {color: '#669999', name: 'app fills buffer'},
  // kBufferQueueQueueStart
  102: {color: '#cccc00', name: 'app queues buffer'},
  // kBufferQueueQueueDone
  103: {color: unusedColor, name: 'buffer is queued'},
  // kBufferQueueAcquire
  104: {color: '#66ffcc', name: 'use buffer'},
  // kBufferQueueReleased
  105: {color: unusedColor, name: 'buffer released'},
  // kBufferFillJank
  106: {color: '#ff0000', name: 'buffer filling jank', width: 1.0, radius: 4.0},

  // kExoSurfaceAttach.
  200: {color: '#99ccff', name: 'surface attach'},
  // kExoProduceResource
  201: {color: '#cc66ff', name: 'produce resource'},
  // kExoBound
  202: {color: '#66ffff', name: 'buffer bound'},
  // kExoPendingQuery
  203: {color: '#00ff99', name: 'pending query'},
  // kExoReleased
  204: {color: unusedColor, name: 'released'},
  // kExoJank
  205: {color: '#ff0000', name: 'surface attach jank', width: 1.0, radius: 4.0},

  // kChromeBarrierOrder.
  300: {color: '#ff9933', name: 'barrier order'},
  // kChromeBarrierFlush
  301: {color: unusedColor, name: 'barrier flush'},

  // kVsync
  400: {color: '#ff3300', name: 'vsync', width: 0.5},
  // kSurfaceFlingerInvalidationStart
  401: {color: '#ff9933', name: 'invalidation start'},
  // kSurfaceFlingerInvalidationDone
  402: {color: unusedColor, name: 'invalidation done'},
  // kSurfaceFlingerCompositionStart
  403: {color: '#3399ff', name: 'composition start'},
  // kSurfaceFlingerCompositionDone
  404: {color: unusedColor, name: 'composition done'},
  // kSurfaceFlingerCompositionJank
  405: {
    color: '#ff0000',
    name: 'Android composition jank',
    width: 1.0,
    radius: 4.0
  },

  // kChromeOSDraw
  500: {color: '#3399ff', name: 'draw'},
  // kChromeOSSwap
  501: {color: '#cc9900', name: 'swap'},
  // kChromeOSWaitForAck
  502: {color: '#ccffff', name: 'wait for ack'},
  // kChromeOSPresentationDone
  503: {color: '#ffbf00', name: 'presentation done'},
  // kChromeOSSwapDone
  504: {color: '#65f441', name: 'swap done'},
  // kChromeOSJank
  505: {
    color: '#ff0000',
    name: 'Chrome composition jank',
    width: 1.0,
    radius: 4.0
  },

  // kCustomEvent
  600: {color: '#7cb342', name: 'Custom event', width: 1.0, radius: 4.0},

  // kInputEventCreated
  700: {color: '#ff6f00', name: 'create'},
  // kInputEventWaylandDispatched
  701: {color: '#f4ff81', name: 'dispatch'},
  // kInputEventDeliverStart
  702: {color: '#388e3c', name: 'deliver start'},
  // kInputEventDeliverEnd
  703: {color: unusedColor, name: 'deliver end'},

  // Service events.
  // kTimeMark
  10000: {color: '#888', name: 'Time mark', width: 0.75},
  // kTimeMarkSmall
  10001: {color: '#888', name: 'Time mark', width: 0.15},
};

/**
 * Defines the map of events that can be treated as the end of event sequence.
 * Time after such events is considered as idle time until the next event
 * starts. Key of |endSequenceEvents| is event type as defined in
 * ArcTracingGraphicsModel::BufferEventType and value is the list of event
 * types that should follow after the tested event to consider it as end of
 * sequence. Empty list means that tested event is certainly end of the
 * sequence.
 */
var endSequenceEvents = {
  // kIdleIn
  0: [],
  // kBufferQueueQueueDone
  103: [],
  // kBufferQueueReleased
  105: [],
  // kExoReleased
  204: [],
  // kChromeBarrierFlush
  301: [],
  // kSurfaceFlingerInvalidationDone
  402: [],
  // kSurfaceFlingerCompositionDone
  404: [],
  // kChromeOSPresentationDone. Chrome does not define exactly which event
  // is the last. Different
  // pipelines may produce different sequences. Both event type may indicate
  // the end of the
  // sequence.
  503: [500 /* kChromeOSDraw */],
  // kChromeOSSwapDone
  504: [500 /* kChromeOSDraw */],
  // kInputEventDeliverEnd
  703: [],
};

/**
 * Keep in sync with ArcValueEvent::Type
 * See chrome/browser/chromeos/arc/tracing/arc_value_event.h.
 * Describes how value events should be rendered in charts. |color| specifies
 * color of the event, |name| is used in tooltips, |width| specify width of
 * the line in chart, |scale| is used to convert actual value to rendered value.
 * When rendered, min and max values and determined and used as a range where
 * chart is drawn. However, in the case range is small, let say 1 mb for
 * |kMemUsed| this may lead to user confusion that huge amount of memory was
 * allocated. To prevent this scanario, |minRange| defines the minimum range of
 * values and is set in scaled units.
 */
var valueAttributes = {
  // kMemUsed.
  1: {
    color: '#ff3d00',
    minRange: 512.0,
    name: 'used mb',
    scale: 1.0 / 1024.0,
    width: 1.0
  },
  // kSwapRead.
  2: {
    color: '#ffc400',
    minRange: 32.0,
    name: 'swap read sectors',
    scale: 1.0,
    width: 1.0
  },
  // kSwapWrite.
  3: {
    color: '#ff9100',
    minRange: 32.0,
    name: 'swap write sectors',
    scale: 1.0,
    width: 1.0
  },
  // kGemObjects.
  5: {
    color: '#3d5afe',
    minRange: 1000,
    name: 'geom. objects',
    scale: 1.0,
    width: 1.0
  },
  // kGemSize.
  6: {
    color: '#7c4dff',
    minRange: 256.0,
    name: 'geom. size mb',
    scale: 1.0 / 1024.0,
    width: 1.0
  },
  // kGpuFreq.
  7: {
    color: '#01579b',
    minRange: 300.0,
    name: 'GPU frequency mhz',
    scale: 1.0,
    width: 1.0
  },
  // kCpuTemp.
  8: {
    color: '#ff3d00',
    minRange: 20.0,
    name: 'CPU celsius.',
    scale: 1.0 / 1000.0,
    width: 1.0
  },
};

/**
 * @type {Object}.
 * Currently loaded model.
 */
var activeModel = null;

/**
 * @type {DetailedInfoView}.
 * Currently active detailed view.
 */
var activeDetailedInfoView = null;

/**
 * Discards active detailed view if it exists.
 */
function discardDetailedInfo() {
  if (activeDetailedInfoView) {
    activeDetailedInfoView.discard();
    activeDetailedInfoView = null;
  }
}

/**
 * Shows detailed view for |eventBand| in response to mouse click event
 * |mouseEvent|.
 */
function showDetailedInfoForBand(eventBand, mouseEvent) {
  discardDetailedInfo();
  activeDetailedInfoView = eventBand.showDetailedInfo(mouseEvent);
  mouseEvent.preventDefault();
}

/**
 * Returns text representation of timestamp in milliseconds with one number
 * after the decimal point.
 *
 * @param {number} timestamp in microseconds.
 */
function timestampToMsText(timestamp) {
  return (timestamp / 1000.0).toFixed(1);
}

/**
 * Changes zoom. |delta| specifies how many zoom levels to adjust. Negative
 * |delta| means zoom in and positive zoom out.
 */
function updateZoom(delta) {
  if (!activeModel) {
    return;
  }
  var newZoomLevel = zoomLevel + delta;
  if (newZoomLevel < 0 || newZoomLevel >= zooms.length) {
    return;
  }

  zoomLevel = newZoomLevel;
  setGraphicBuffersModel(activeModel);
}

/**
 * Initialises UI by setting keyboard and mouse listeners to discard detailed
 * view overlay.
 */
function initializeUi() {
  document.body.onkeydown = function(event) {
    // Escape and Enter.
    if (event.key === 'Escape' || event.key === 'Enter') {
      discardDetailedInfo();
    } else if (event.key === 'w') {
      // Zoom in.
      updateZoom(-1 /* delta */);
    } else if (event.key === 's') {
      // Zoom out.
      updateZoom(1 /* delta */);
    }
  };

  window.onclick = function(event) {
    // Detect click outside the detailed view.
    if (event.defaultPrevented || activeDetailedInfoView == null) {
      return;
    }
    if (!activeDetailedInfoView.overlay.contains(event.target)) {
      discardDetailedInfo();
    }
  };

  $('arc-graphics-tracing-save').onclick = function(event) {
    var linkElement = document.createElement('a');
    var file = new Blob([JSON.stringify(activeModel)], {type: 'text/plain'});
    linkElement.href = URL.createObjectURL(file);
    linkElement.download = 'tracing_model.json';
    linkElement.click();
  };

  $('arc-graphics-tracing-load').onclick = function(event) {
    var fileElement = document.createElement('input');
    fileElement.type = 'file';

    fileElement.onchange = function(event) {
      var reader = new FileReader();
      reader.onload = function(response) {
        chrome.send('loadFromText', [response.target.result]);
      };
      reader.readAsText(event.target.files[0]);
    };

    fileElement.click();
  };
}

/** Factory class for SVG elements. */
class SVG {
  // Creates rectangle element in the |svg| with provided attributes.
  static addRect(svg, x, y, width, height, color, opacity) {
    var rect = document.createElementNS(svgNS, 'rect');
    rect.setAttributeNS(null, 'x', x);
    rect.setAttributeNS(null, 'y', y);
    rect.setAttributeNS(null, 'width', width);
    rect.setAttributeNS(null, 'height', height);
    rect.setAttributeNS(null, 'fill', color);
    rect.setAttributeNS(null, 'stroke', 'none');
    if (opacity) {
      rect.setAttributeNS(null, 'fill-opacity', opacity);
    }
    svg.appendChild(rect);
    return rect;
  }

  // Creates line element in the |svg| with provided attributes.
  static addLine(svg, x1, y1, x2, y2, color, width) {
    var line = document.createElementNS(svgNS, 'line');
    line.setAttributeNS(null, 'x1', x1);
    line.setAttributeNS(null, 'y1', y1);
    line.setAttributeNS(null, 'x2', x2);
    line.setAttributeNS(null, 'y2', y2);
    line.setAttributeNS(null, 'stroke', color);
    line.setAttributeNS(null, 'stroke-width', width);
    svg.appendChild(line);
    return line;
  }

  // Creates polyline element in the |svg| with provided attributes.
  static addPolyline(svg, points, color, width) {
    var polyline = document.createElementNS(svgNS, 'polyline');
    polyline.setAttributeNS(null, 'points', points.join(' '));
    polyline.setAttributeNS(null, 'stroke', color);
    polyline.setAttributeNS(null, 'stroke-width', width);
    polyline.setAttributeNS(null, 'fill', 'none');
    svg.appendChild(polyline);
    return polyline;
  }

  // Creates circle element in the |svg| with provided attributes.
  static addCircle(svg, x, y, radius, strokeWidth, color, strokeColor) {
    var circle = document.createElementNS(svgNS, 'circle');
    circle.setAttributeNS(null, 'cx', x);
    circle.setAttributeNS(null, 'cy', y);
    circle.setAttributeNS(null, 'r', radius);
    circle.setAttributeNS(null, 'fill', color);
    circle.setAttributeNS(null, 'stroke', strokeColor);
    circle.setAttributeNS(null, 'stroke-width', strokeWidth);
    svg.appendChild(circle);
    return circle;
  }

  // Creates text element in the |svg| with provided attributes.
  static addText(svg, x, y, fontSize, textContent, anchor, transform) {
    var text = document.createElementNS(svgNS, 'text');
    text.setAttributeNS(null, 'x', x);
    text.setAttributeNS(null, 'y', y);
    text.setAttributeNS(null, 'fill', 'black');
    text.setAttributeNS(null, 'font-size', fontSize);
    if (anchor) {
      text.setAttributeNS(null, 'text-anchor', anchor);
    }
    if (transform) {
      text.setAttributeNS(null, 'transform', transform);
    }
    text.appendChild(document.createTextNode(textContent));
    svg.appendChild(text);
    return text;
  }
}

/**
 * Represents title for events bands that can collapse/expand controlled
 * content.
 */
class EventBandTitle {
  constructor(parent, title, className, opt_iconContent) {
    this.div = document.createElement('div');
    this.div.classList.add(className);
    if (opt_iconContent) {
      var icon = document.createElement('img');
      icon.src = 'data:image/png;base64,' + opt_iconContent;
      this.div.appendChild(icon);
    }
    var span = document.createElement('span');
    span.appendChild(document.createTextNode(title));
    this.div.appendChild(span);
    this.controlledItems = [];
    this.div.onclick = this.onClick_.bind(this);
    this.parent = parent;
    this.parent.appendChild(this.div);
  }

  /**
   * Adds extra HTML element under the control. This element will be
   * automatically expanded/collapsed together with this title.
   *
   * @param {HTMLElement} item svg element to control.
   */
  addContolledItems(item) {
    this.controlledItems.push(item);
  }

  onClick_() {
    this.div.classList.toggle('hidden');
    for (var i = 0; i < this.controlledItems.length; ++i) {
      this.controlledItems[i].classList.toggle('hidden');
    }
  }
}

/** Represents container for event bands. */
class EventBands {
  /**
   * Creates container for the event bands.
   *
   * @param {EventBandTitle} title controls visibility of this band.
   * @param {string} className class name of the svg element that represents
   *     this band. 'arc-events-top-band' is used for top-level events and
   *     'arc-events-inner-band' is used for per-buffer events.
   * @param {number} resolution the resolution of bands microseconds per 1
   *     pixel.
   * @param {number} minTimestamp the minimum timestamp to display on bands.
   * @param {number} minTimestamp the maximum timestamp to display on bands.
   */
  constructor(title, className, resolution, minTimestamp, maxTimestamp) {
    // Keep information about bands and charts and their bounds.
    this.bands = [];
    this.charts = [];
    this.globalEvents = [];
    this.vsyncEvents = null;
    this.resolution = resolution;
    this.minTimestamp = minTimestamp;
    this.maxTimestamp = maxTimestamp;
    this.height = 0;
    // Offset of the next band of events.
    this.nextYOffset = 0;
    this.svg = document.createElementNS(svgNS, 'svg');
    this.svg.setAttributeNS(
        'http://www.w3.org/2000/xmlns/', 'xmlns:xlink',
        'http://www.w3.org/1999/xlink');
    this.setBandOffsetX(0);
    this.setWidth(0);
    this.svg.setAttribute('height', this.height + 'px');
    this.svg.classList.add(className);

    this.setTooltip_();
    title.addContolledItems(this.svg);
    title.parent.appendChild(this.svg);

    // Set of constants, used for rendering content.
    this.fontSize = 12;
    this.verticalGap = 5;
    this.horizontalGap = 10;
    this.lineHeight = 16;
    this.iconOffset = 24;
    this.iconRadius = 4;
    this.textOffset = 32;
  }

  /**
   * Sets the horizontal offset to render bands.
   * @param {number} offsetX offset in pixels.
   */
  setBandOffsetX(offsetX) {
    this.bandOffsetX = offsetX;
  }

  /**
   * Sets the widths of event bands.
   * @param {number} width width in pixels.
   */
  setWidth(width) {
    this.width = width;
    this.svg.setAttribute('width', this.width + 'px');
  }

  /**
   * Converts timestamp into pixel offset. 1 pixel corresponds resolution
   * microseconds.
   *
   * @param {number} timestamp in microseconds.
   */
  timestampToOffset(timestamp) {
    return (timestamp - this.minTimestamp) / this.resolution;
  }

  /**
   * Opposite conversion of |timestampToOffset|
   *
   * @param {number} offset in pixels.
   */
  offsetToTime(offset) {
    return offset * this.resolution + this.minTimestamp;
  }

  /**
   * This adds new band of events. Height of svg container is automatically
   * adjusted to fit the new content.
   *
   * @param {Events} eventBand event band to add.
   * @param {number} height of the band.
   * @param {number} padding to separate from the next band or chart.
   */
  addBand(eventBand, height, padding) {
    var currentColor = unusedColor;
    var addToBand = false;
    var x = this.bandOffsetX;
    var eventIndex = eventBand.getFirstAfter(this.minTimestamp);
    while (eventIndex >= 0) {
      var event = eventBand.events[eventIndex];
      if (event[1] >= this.maxTimestamp) {
        break;
      }
      var nextX = this.timestampToOffset(event[1]) + this.bandOffsetX;
      if (addToBand) {
        SVG.addRect(
            this.svg, x, this.nextYOffset, nextX - x, height, currentColor);
      }
      if (eventBand.isEndOfSequence(eventIndex)) {
        currentColor = unusedColor;
        addToBand = false;
      } else {
        currentColor = eventAttributes[event[0]].color;
        addToBand = true;
      }
      x = nextX;
      eventIndex = eventBand.getNextEvent(eventIndex, 1 /* direction */);
    }
    if (addToBand) {
      SVG.addRect(
          this.svg, x, this.nextYOffset,
          this.timestampToOffset(this.maxTimestamp) - x + this.bandOffsetX,
          height, currentColor);
    }

    this.bands.push({
      band: eventBand,
      top: this.nextYOffset,
      bottom: this.nextYOffset + height
    });

    this.updateHeight(height, padding);
  }

  /**
   * This adds horizontal separator at |nextYOffset|.
   *
   * @param {number} padding to separate from the next band or chart.
   */
  addBandSeparator(padding) {
    SVG.addLine(
        this.svg, 0, this.nextYOffset, this.width, this.nextYOffset, '#888',
        0.25);
    this.updateHeight(0 /* height */, padding);
  }

  /**
   * This adds new chart. Height of svg container is automatically adjusted to
   * fit the new content. This creates empty chart and one or more calls
   * |addChartSources| are expected to add actual content to the chart.
   *
   * @param {number} height of the chart.
   * @param {number} padding to separate from the next band or chart.
   */
  addChart(height, padding) {
    this.charts.push({
      sourcesWithBounds: [],
      top: this.nextYOffset,
      bottom: this.nextYOffset + height
    });

    this.updateHeight(height, padding);
  }

  /**
   * This adds new chart into existing area.
   * @param {number} top position of chart.
   * @param {number} bottom position of chart.
   */
  addChartToExistingArea(top, bottom) {
    this.charts.push({sourcesWithBounds: [], top: top, bottom: bottom});
  }


  /**
   * This adds sources of events to the last chart.
   *
   * @param {Events[]} sources is array of groupped source of events to add.
   *     These events are logically linked to each other and represented as a
   *     separate line.
   * @param {boolean} smooth if set to true then indicates that chart should
   *                  display value interpolated, otherwise values are changed
   *                  discretely.
   */
  addChartSources(sources, smooth) {
    var chart = this.charts[this.charts.length - 1];

    // Calculate min/max for sources and event indices.
    var minValue = null;
    var maxValue = null;
    var attributes = null;
    var eventIndicesForAll = [];
    for (var i = 0; i < sources.length; ++i) {
      var source = sources[i];
      var eventIndex = source.getFirstAfter(this.minTimestamp);
      if (eventIndex < 0 || source.events[eventIndex][1] > this.maxTimestamp) {
        eventIndicesForAll.push([]);
        continue;
      }
      if (!minValue) {
        minValue = source.events[eventIndex][2];
        maxValue = source.events[eventIndex][2];
      }
      var eventIndices = [];
      while (eventIndex >= 0 &&
             source.events[eventIndex][1] <= this.maxTimestamp) {
        eventIndices.push(eventIndex);
        minValue = Math.min(minValue, source.events[eventIndex][2]);
        maxValue = Math.max(maxValue, source.events[eventIndex][2]);
        if (!attributes) {
          attributes = valueAttributes[source.events[eventIndex][0]];
        }
        eventIndex = source.getNextEvent(eventIndex, 1 /* direction */);
      }
      eventIndicesForAll.push(eventIndices);
    }

    if (!attributes) {
      // no one event to render.
      return;
    }

    // Ensure minimum value range.
    if (maxValue - minValue < attributes.minRange / attributes.scale) {
      maxValue = minValue + attributes.minRange / attributes.scale;
    }

    // Add +-1% to bounds.
    var dif = maxValue - minValue;
    minValue -= dif * 0.01;
    maxValue += dif * 0.01;
    var divider = 1.0 / (maxValue - minValue);

    // Render now.
    var height = chart.bottom - chart.top;
    for (var i = 0; i < sources.length; ++i) {
      var source = sources[i];
      var eventIndices = eventIndicesForAll[i];
      if (eventIndices.length == 0) {
        continue;
      }
      // Determine type using first element.
      var eventType = source.events[eventIndices[0]][0];

      var points = [];
      var lastY = 0;
      for (var j = 0; j < eventIndices.length; ++j) {
        var event = source.events[eventIndices[j]];
        var x = this.timestampToOffset(event[1]);
        var y = height * (maxValue - event[2]) * divider;
        if (!smooth && j != 0) {
          points.push([x, lastY]);
        }
        points.push([x, y]);
        lastY = y;
      }

      chart.sourcesWithBounds.push({
        attributes: attributes,
        minValue: minValue,
        maxValue: maxValue,
        source: source,
        smooth: smooth
      });

      SVG.addPolyline(this.svg, points, attributes.color, attributes.width);
    }
  }

  /**
   * Updates height of svg container.
   *
   * @param {number} new height of the chart.
   * @param {number} padding to separate from the next band or chart.
   */
  updateHeight(height, padding) {
    this.nextYOffset += height;
    this.height = this.nextYOffset;
    this.svg.setAttribute('height', this.height + 'px');
    this.nextYOffset += padding;
  }

  /**
   * This adds events as a global events that do not belong to any band.
   *
   * @param {Events} events to add.
   * @param {string} renderType defines how to render events, can be underfined
   *                 for default or set to 'circle'.
   */
  addGlobal(events, renderType) {
    var eventIndex = -1;
    while (true) {
      eventIndex = events.getNextEvent(eventIndex, 1 /* direction */);
      if (eventIndex < 0) {
        break;
      }
      var event = events.events[eventIndex];
      var attributes = events.getEventAttributes(eventIndex);
      var x = this.timestampToOffset(event[1]) + this.bandOffsetX;
      if (renderType == 'circle') {
        SVG.addCircle(
            this.svg, x, this.height / 2, attributes.radius,
            1 /* strokeWidth */, attributes.color, 'black' /* strokeColor */);
      } else {
        SVG.addLine(
            this.svg, x, 0, x, this.height, attributes.color, attributes.width);
      }
    }
    this.globalEvents.push(events);
  }

  /**
   * Sets VSYNC events and adds them as a global events.
   *
   * @param {Events} VSYNC events to set.
   */
  setVSync(events) {
    this.addGlobal(events);
    this.vsyncEvents = events;
  }

  /** Initializes tooltip support by observing mouse events */
  setTooltip_() {
    this.tooltip = $('arc-event-band-tooltip');
    this.svg.onmouseover = this.showToolTip_.bind(this);
    this.svg.onmouseout = this.hideToolTip_.bind(this);
    this.svg.onmousemove = this.updateToolTip_.bind(this);
    this.svg.onclick = (event) => {
      showDetailedInfoForBand(this, event);
    };
  }

  /** Updates tooltip and shows it for this band. */
  showToolTip_(event) {
    this.updateToolTip_(event);
  }

  /** Hides the tooltip. */
  hideToolTip_() {
    this.tooltip.classList.remove('active');
  }

  /**
   * Finds global events around |timestamp| and not farther than |distance|.
   *
   * @param {number} timestamp to search.
   * @param {number} distance to search.
   * @returns {Array} array of events sorted by distance to |timestamp| from
   *                  closest to farthest.
   */
  findGlobalEvents_(timestamp, distance) {
    var events = [];
    var leftBorder = timestamp - distance;
    var rightBorder = timestamp + distance;
    for (var i = 0; i < this.globalEvents.length; ++i) {
      var index = this.globalEvents[i].getFirstAfter(leftBorder);
      while (index >= 0) {
        var event = this.globalEvents[i].events[index];
        if (event[1] > rightBorder) {
          break;
        }
        events.push(event);
        index = this.globalEvents[i].getNextEvent(index, 1 /* direction */);
      }
    }
    events.sort(function(a, b) {
      var distanceA = Math.abs(timestamp - a[1]);
      var distanceB = Math.abs(timestamp - b[1]);
      return distanceA - distanceB;
    });
    return events;
  }

  /**
   * Updates tool tip based on event under the current cursor.
   *
   * @param {Object} event mouse event.
   */
  updateToolTip_(event) {
    // Clear previous content.
    this.tooltip.textContent = '';

    var svgStyle = window.getComputedStyle(this.svg, null);
    var paddingLeft = parseFloat(svgStyle.getPropertyValue('padding-left'));
    var paddingTop = parseFloat(svgStyle.getPropertyValue('padding-top'));
    var eventX = event.offsetX - paddingLeft;
    var eventY = event.offsetY - paddingTop;

    if (eventX < this.bandOffsetX) {
      this.tooltip.classList.remove('active');
      return;
    }

    var svg = document.createElementNS(svgNS, 'svg');
    svg.setAttributeNS(
        'http://www.w3.org/2000/xmlns/', 'xmlns:xlink',
        'http://www.w3.org/1999/xlink');
    this.tooltip.appendChild(svg);

    var eventTimestamp = this.offsetToTime(eventX - this.bandOffsetX);

    var updated = false;
    var width = 220;
    var yOffset = this.verticalGap;

    // In case of global events are not available, render tooltip for band and
    // chart.
    yOffset =
        this.updateToolTipForGlobalEvents_(event, svg, eventTimestamp, yOffset);
    if (yOffset == this.verticalGap) {
      // Find band for this mouse event.
      for (var i = 0; i < this.bands.length; ++i) {
        if (this.bands[i].top <= eventY && this.bands[i].bottom > eventY) {
          yOffset = this.updateToolTipForBand_(
              event, svg, eventTimestamp, this.bands[i].band, yOffset);
          break;
        }
      }

      // Find chart for this mouse event.
      for (var i = 0; i < this.charts.length; ++i) {
        if (this.charts[i].top <= eventY && this.charts[i].bottom > eventY) {
          yOffset = this.updateToolTipForChart_(
              event, svg, eventTimestamp, this.charts[i], yOffset);
          break;
        }
      }
    }

    if (yOffset > this.verticalGap) {
      // Content was added.
      if (this.canShowDetailedInfo()) {
        yOffset += this.lineHeight;
        SVG.addText(
            svg, this.horizontalGap, yOffset, this.fontSize,
            'Click for detailed info');
      }

      yOffset += this.verticalGap;

      this.showTooltipForEvent_(event, svg, yOffset, width);
    } else {
      this.tooltip.classList.remove('active');
    }
  }

  /**
   * Returns timestamp of the last VSYNC event happened before or on given
   * |eventTimestamp|.
   *
   * @param {number} eventTimestamp.
   */
  getVSyncTimestamp_(eventTimestamp) {
    if (!this.vsyncEvents) {
      return null;
    }
    var vsyncEventIndex = this.vsyncEvents.getLastBefore(eventTimestamp);
    if (vsyncEventIndex < 0) {
      return null;
    }
    return this.vsyncEvents.events[vsyncEventIndex][1];
  }


  /**
   * Adds time information for |eventTimestamp| to the tooltip. Global time is
   * added first and VSYNC relative time is added next in case VSYNC event could
   * be found.
   *
   * @param {Object} svg tooltip container.
   * @param {number} yOffset current vertical offset.
   * @param {number} eventTimestamp timestamp of the event.
   * @returns {number} vertical position of the next element.
   */
  addTimeInfoToTooltip_(svg, yOffset, eventTimestamp) {
    var vsyncTimestamp = this.getVSyncTimestamp_(eventTimestamp);

    var text = timestampToMsText(eventTimestamp) + ' ms';
    if (vsyncTimestamp) {
      text += ', +' + timestampToMsText(eventTimestamp - vsyncTimestamp) +
          ' since last vsync';
    }

    yOffset += this.lineHeight;
    SVG.addText(svg, this.horizontalGap, yOffset, this.fontSize, text);

    return yOffset;
  }

  /**
   * Creates and shows tooltip for global events in case they are found around
   * mouse event position.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForGlobalEvents_(event, svg, eventTimestamp, yOffset) {
    // Try to find closest global events in the range -3..3 pixels. Several
    // events may stick close each other so let diplay up to 3 closest events.
    var distanceMcs = 3 * this.resolution;
    var globalEvents = this.findGlobalEvents_(eventTimestamp, distanceMcs);
    if (globalEvents.length == 0) {
      return yOffset;
    }

    // Show the global events info.
    var globalEventCnt = Math.min(globalEvents.length, 3);
    for (var i = 0; i < globalEventCnt; ++i) {
      var globalEvent = globalEvents[i];
      var globalEventType = globalEvent[0];
      var globalEventTimestamp = globalEvent[1];
      if (globalEventType == 400 /* kVsync */) {
        // -1 to prevent VSYNC detects itself. In last case, previous VSYNC
        // would be chosen.
        globalEventTimestamp -= 1;
      }

      yOffset = this.addTimeInfoToTooltip_(svg, yOffset, globalEventTimestamp);

      var attributes = eventAttributes[globalEventType];
      yOffset += this.lineHeight;
      SVG.addCircle(
          svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
          attributes.color, 'black');
      SVG.addText(
          svg, this.textOffset, yOffset, this.fontSize, attributes.name);

      // Render content if exists.
      if (globalEvent.length > 2) {
        yOffset += this.lineHeight;
        SVG.addText(
            svg, this.textOffset + this.horizontalGap, yOffset, this.fontSize,
            globalEvent[2]);
      }
    }

    yOffset += this.verticalGap;

    return yOffset;
  }

  /**
   * Creates and shows tooltip for event band for the position under |event|.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {Object} active event band.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForBand_(event, svg, eventTimestamp, eventBand, yOffset) {
    // Find the event under the cursor. |index| points to the current event
    // and |nextIndex| points to the next event.
    var nextIndex = eventBand.getFirstEvent();
    while (nextIndex >= 0) {
      if (eventBand.events[nextIndex][1] > eventTimestamp) {
        break;
      }
      nextIndex = eventBand.getNextEvent(nextIndex, 1 /* direction */);
    }
    var index = eventBand.getNextEvent(nextIndex, -1 /* direction */);

    if (index < 0 || eventBand.isEndOfSequence(index)) {
      // In case cursor points to idle event, show its interval.
      yOffset += this.lineHeight;
      var startIdle = index < 0 ? 0 : eventBand.events[index][1];
      var endIdle =
          nextIndex < 0 ? this.maxTimestamp : eventBand.events[nextIndex][1];
      SVG.addText(
          svg, this.horizontalGap, yOffset, this.fontSize,
          'Idle ' + timestampToMsText(startIdle) + '...' +
              timestampToMsText(endIdle) + ' chart time ms.');
    } else {
      // Show the sequence of non-idle events.
      // Find the start of the non-idle sequence.
      while (true) {
        var prevIndex = eventBand.getNextEvent(index, -1 /* direction */);
        if (prevIndex < 0 || eventBand.isEndOfSequence(prevIndex)) {
          break;
        }
        index = prevIndex;
      }

      var sequenceTimestamp = eventBand.events[index][1];
      yOffset = this.addTimeInfoToTooltip_(svg, yOffset, sequenceTimestamp);

      var lastTimestamp = sequenceTimestamp;
      // Scan for the entries to show.
      var entriesToShow = [];
      while (index >= 0) {
        var attributes = eventBand.getEventAttributes(index);
        var eventTimestamp = eventBand.events[index][1];
        var entryToShow = {};
        entryToShow.color = attributes.color;
        if (eventBand.events[index].length > 2) {
          entryToShow.text = attributes.name + ' ' + eventBand.events[index][2];
        } else {
          entryToShow.text = attributes.name;
        }
        if (entriesToShow.length > 0) {
          entriesToShow[entriesToShow.length - 1].text +=
              ' [' + timestampToMsText(eventTimestamp - lastTimestamp) + ' ms]';
        }
        entriesToShow.push(entryToShow);
        if (eventBand.isEndOfSequence(index)) {
          break;
        }
        lastTimestamp = eventTimestamp;
        index = eventBand.getNextEvent(index, 1 /* direction */);
      }

      // Last element is end of sequence, use bandColor for the icon.
      if (entriesToShow.length > 0) {
        entriesToShow[entriesToShow.length - 1].color = bandColor;
      }
      for (var i = 0; i < entriesToShow.length; ++i) {
        var entryToShow = entriesToShow[i];
        yOffset += this.lineHeight;
        SVG.addCircle(
            svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
            entryToShow.color, 'black');
        SVG.addText(
            svg, this.textOffset, yOffset, this.fontSize, entryToShow.text);
      }
    }

    yOffset += this.verticalGap;

    return yOffset;
  }

  /**
   * Creates and show tooltip for event chart for the position under |event|.
   *
   * @param {Object} mouse event.
   * @param {Object} svg tooltip content.
   * @param (number} eventTimestamp timestamp of event.
   * @param {Object} active event chart.
   * @param {number} yOffset starting vertical position to fill this content.
   * @returns {number} next vertical position to fill the next content.
   */
  updateToolTipForChart_(event, svg, eventTimestamp, chart, yOffset) {
    var valueOffset = 32;

    var contentAdded = false;

    for (var i = 0; i < chart.sourcesWithBounds.length; ++i) {
      var sourceWithBounds = chart.sourcesWithBounds[i];
      // Interpolate results.
      var indexAfter = sourceWithBounds.source.getFirstAfter(eventTimestamp);
      if (indexAfter < 0) {
        continue;
      }
      var indexBefore =
          sourceWithBounds.source.getNextEvent(indexAfter, -1 /* direction */);
      if (indexBefore < 0) {
        continue;
      }
      var eventBefore = sourceWithBounds.source.events[indexBefore];
      var eventAfter = sourceWithBounds.source.events[indexAfter];
      var factor =
          (eventTimestamp - eventBefore[1]) / (eventAfter[1] - eventBefore[1]);

      if (!sourceWithBounds.smooth) {
        // Clamp to before value.
        if (factor < 1.0) {
          factor = 0.0;
        }
      }
      var value = factor * eventAfter[2] + (1.0 - factor) * eventBefore[2];

      if (!contentAdded) {
        yOffset = this.addTimeInfoToTooltip_(svg, yOffset, eventTimestamp);
        contentAdded = true;
      }

      yOffset += this.lineHeight;
      SVG.addCircle(
          svg, this.iconOffset, yOffset - this.iconRadius, this.iconRadius, 1,
          sourceWithBounds.attributes.color, 'black');
      var text = (value * sourceWithBounds.attributes.scale).toFixed(1) + ' ' +
          sourceWithBounds.attributes.name;
      SVG.addText(svg, valueOffset, yOffset, this.fontSize, text);
    }

    if (contentAdded) {
      yOffset += this.verticalGap;
    }

    return yOffset;
  }

  /**
   * Helper that shows tooltip after filling its content.
   *
   * @param {Object} mouse event, used to determine the position of tooltip.
   * @param {Object} svg content of tooltip.
   * @param {number} height of the tooltip view.
   * @param {number} width of the tooltip view.
   */
  showTooltipForEvent_(event, svg, height, width) {
    svg.setAttribute('height', height + 'px');
    svg.setAttribute('width', width + 'px');
    this.tooltip.style.left = event.clientX + 'px';
    this.tooltip.style.top = event.clientY + 'px';
    this.tooltip.style.height = height + 'px';
    this.tooltip.style.width = width + 'px';
    this.tooltip.classList.add('active');
  }

  /**
   * Returns true in case band can show detailed info.
   */
  canShowDetailedInfo() {
    return false;
  }

  /**
   * Shows detailed info for the position under mouse event |event|. By default
   * it creates nothing.
   */
  showDetailedInfo(event) {
    return null;
  }
}

/**
 * Base class for detailed info view.
 */
class DetailedInfoView {
  discard() {}
}

/**
 * CPU detailed info view. Renders 4x zoomed CPU events split by processes and
 * threads.
 */
class CpuDetailedInfoView extends DetailedInfoView {
  create(overviewBand) {
    this.overlay = $('arc-detailed-view-overlay');
    var overviewRect = overviewBand.svg.getBoundingClientRect();

    // Clear previous content.
    this.overlay.textContent = '';

    // UI constants to render.
    var columnNameWidth = 130;
    var columnUsageWidth = 40;
    var scrollBarWidth = 3;
    var zoomFactor = 4.0;
    var cpuBandHeight = 14;
    var processInfoHeight = 14;
    var padding = 2;
    var fontSize = 12;
    var processInfoPadding = 2;
    var threadInfoPadding = 12;
    var cpuUsagePadding = 2;
    var columnsWidth = columnNameWidth + columnUsageWidth;

    // Use minimum 80% of inner width or 600 pixels to display detailed view
    // zoomed |zoomFactor| times.
    var availableWidthPixels =
        window.innerWidth * 0.8 - columnsWidth - scrollBarWidth;
    availableWidthPixels = Math.max(availableWidthPixels, 600);
    var availableForHalfBandMcs = Math.floor(
        overviewBand.offsetToTime(availableWidthPixels) / (2.0 * zoomFactor));
    // Determine the interval to display.
    var eventTimestamp =
        overviewBand.offsetToTime(event.offsetX - overviewBand.bandOffsetX);
    var minTimestamp = eventTimestamp - availableForHalfBandMcs;
    var maxTimestamp = eventTimestamp + availableForHalfBandMcs + 1;
    var duration = maxTimestamp - minTimestamp;

    // Construct sub-model of active/idle events per each thread, active within
    // this interval.
    var eventsPerTid = {};
    for (var cpuId = 0; cpuId < overviewBand.model.system.cpu.length; cpuId++) {
      var activeEvents = new Events(
          overviewBand.model.system.cpu[cpuId], 3 /* kActive */,
          3 /* kActive */);
      // Assume we have an idle before minTimestamp.
      var activeTid = 0;
      var index = activeEvents.getFirstAfter(minTimestamp);
      var activeStartTimestamp = minTimestamp;
      // Check if previous event goes over minTimestamp, in that case extract
      // the active thread.
      if (index > 0) {
        var lastBefore = activeEvents.getNextEvent(index, -1 /* direction */);
        if (lastBefore >= 0) {
          // This may be idle (tid=0) or real thread.
          activeTid = activeEvents.events[lastBefore][2];
        }
      }
      while (index >= 0 && activeEvents.events[index][1] < maxTimestamp) {
        this.addActivityTime_(
            eventsPerTid, activeTid, activeStartTimestamp,
            activeEvents.events[index][1]);
        activeTid = activeEvents.events[index][2];
        activeStartTimestamp = activeEvents.events[index][1];
        index = activeEvents.getNextEvent(index, 1 /* direction */);
      }
      this.addActivityTime_(
          eventsPerTid, activeTid, activeStartTimestamp, maxTimestamp - 1);
    }

    // The same thread might be executed on different CPU cores. Sort events.
    for (var tid in eventsPerTid) {
      eventsPerTid[tid].events.sort(function(a, b) {
        return a[1] - b[1];
      });
    }

    // Group threads by process.
    var threadsPerPid = {};
    var pids = [];
    var totalTime = 0;
    for (var tid in eventsPerTid) {
      var thread = eventsPerTid[tid];
      var pid = overviewBand.model.system.threads[tid].pid;
      if (!(pid in threadsPerPid)) {
        pids.push(pid);
        threadsPerPid[pid] = {};
        threadsPerPid[pid].totalTime = 0;
        threadsPerPid[pid].threads = [];
      }
      threadsPerPid[pid].totalTime += thread.totalTime;
      threadsPerPid[pid].threads.push(thread);
      totalTime += thread.totalTime;
    }

    // Sort processes per time usage.
    pids.sort(function(a, b) {
      return threadsPerPid[b].totalTime - threadsPerPid[a].totalTime;
    });

    var totalUsage = 100.0 * totalTime / duration;
    var cpuInfo = 'CPU view. ' + pids.length + '/' +
        Object.keys(eventsPerTid).length +
        ' active processes/threads. Total cpu usage: ' + totalUsage.toFixed(2) +
        '%.';
    var title = new EventBandTitle(this.overlay, cpuInfo, 'arc-cpu-view-title');
    var bands = new EventBands(
        title, 'arc-events-cpu-detailed-band',
        overviewBand.resolution / zoomFactor, minTimestamp, maxTimestamp);
    bands.setBandOffsetX(columnsWidth);
    var bandsWidth = bands.timestampToOffset(maxTimestamp);
    var totalWidth = bandsWidth + columnsWidth;
    bands.setWidth(totalWidth);

    for (var i = 0; i < pids.length; i++) {
      var pid = pids[i];
      var threads = threadsPerPid[pid].threads;
      var processName;
      if (pid in overviewBand.model.system.threads) {
        processName = overviewBand.model.system.threads[pid].name;
      } else {
        processName = 'Others';
      }
      var processCpuUsage = 100.0 * threadsPerPid[pid].totalTime / duration;
      var processInfo = processName + ' <' + pid + '>';
      var processInfoTextLine = bands.nextYOffset + processInfoHeight - padding;
      SVG.addText(
          bands.svg, processInfoPadding, processInfoTextLine, fontSize,
          processInfo);
      SVG.addText(
          bands.svg, columnsWidth - cpuUsagePadding, processInfoTextLine,
          fontSize, processCpuUsage.toFixed(2), 'end' /* anchor */);

      // Sort threads per time usage.
      threads.sort(function(a, b) {
        return eventsPerTid[b.tid].totalTime - eventsPerTid[a.tid].totalTime;
      });

      // In case we have only one main thread add CPU info to process.
      if (threads.length == 1 && threads[0].tid == pid) {
        bands.addBand(
            new Events(eventsPerTid[pid].events, 0, 1), cpuBandHeight, padding);
        bands.addBandSeparator(2 /* padding */);
        continue;
      }

      bands.nextYOffset += (processInfoHeight + padding);

      for (var j = 0; j < threads.length; j++) {
        var tid = threads[j].tid;
        bands.addBand(
            new Events(eventsPerTid[tid].events, 0, 1), cpuBandHeight, padding);
        var threadName = overviewBand.model.system.threads[tid].name;
        var threadCpuUsage = 100.0 * threads[j].totalTime / duration;
        SVG.addText(
            bands.svg, threadInfoPadding, bands.nextYOffset - padding, fontSize,
            threadName);
        SVG.addText(
            bands.svg, columnsWidth - cpuUsagePadding,
            bands.nextYOffset - 2 * padding, fontSize,
            threadCpuUsage.toFixed(2), 'end' /* anchor */);
      }
      bands.addBandSeparator(2 /* padding */);
    }

    var vsyncEvents = new Events(
        overviewBand.model.android.global_events, 400 /* kVsync */,
        400 /* kVsync */);
    bands.setVSync(vsyncEvents);

    // Add center and boundary lines.
    var kTimeMark = 10000;
    var timeEvents = [
      [kTimeMark, minTimestamp], [kTimeMark, eventTimestamp],
      [kTimeMark, maxTimestamp - 1]
    ];
    bands.addGlobal(new Events(timeEvents, kTimeMark, kTimeMark));

    SVG.addLine(
        bands.svg, columnNameWidth, 0, columnNameWidth, bands.height, '#888',
        0.25);

    SVG.addLine(
        bands.svg, columnsWidth, 0, columnsWidth, bands.height, '#888', 0.25);

    // Mark zoomed interval in overview.
    var overviewX = overviewBand.timestampToOffset(minTimestamp);
    var overviewWidth =
        overviewBand.timestampToOffset(maxTimestamp) - overviewX;
    this.bandSelection = SVG.addRect(
        overviewBand.svg, overviewX, 0, overviewWidth, overviewBand.height,
        '#000' /* color */, 0.1 /* opacity */);
    // Prevent band selection to capture mouse events that would lead to
    // incorrect anchor position computation for detailed view.
    this.bandSelection.classList.add('arc-no-mouse-events');

    // Align position in overview and middle line here if possible.
    var left = Math.max(
        Math.min(
            Math.round(event.clientX - columnsWidth - bandsWidth * 0.5),
            window.innerWidth - totalWidth),
        0);
    this.overlay.style.left = left + 'px';
    // Place below the overview with small gap.
    this.overlay.style.top = (overviewRect.bottom + window.scrollY + 2) + 'px';
    this.overlay.classList.add('active');
  }

  discard() {
    this.overlay.classList.remove('active');
    this.bandSelection.remove();
  }

  /**
   * Helper that adds kIdleIn/kIdleOut events into the dictionary.
   *
   * @param {Object} eventsPerTid dictionary to fill. Key is thread id and
   *     value is object that contains all events for thread with related
   *     information.
   * @param {number} tid thread id.
   * @param {number} timestampFrom start time of thread activity.
   * @param {number} timestampTo end time of thread activity.
   */
  addActivityTime_(eventsPerTid, tid, timestampFrom, timestampTo) {
    if (tid == 0) {
      // Don't process idle thread.
      return;
    }
    if (!(tid in eventsPerTid)) {
      // Create the band for the new thread.
      eventsPerTid[tid] = {};
      eventsPerTid[tid].totalTime = 0;
      eventsPerTid[tid].events = [];
      eventsPerTid[tid].tid = tid;
    }
    eventsPerTid[tid].events.push([1 /* kIdleOut */, timestampFrom]);
    eventsPerTid[tid].events.push([0 /* kIdleIn */, timestampTo]);
    // Update total time for this thread.
    eventsPerTid[tid].totalTime += (timestampTo - timestampFrom);
  }
}

class CpuEventBands extends EventBands {
  setModel(model) {
    this.model = model;
    var bandHeight = 6;
    var padding = 2;
    for (var cpuId = 0; cpuId < this.model.system.cpu.length; cpuId++) {
      this.addBand(
          new Events(this.model.system.cpu[cpuId], 0, 1), bandHeight, padding);
    }
  }

  canShowDetailedInfo() {
    return true;
  }

  showDetailedInfo(event) {
    var view = new CpuDetailedInfoView();
    view.create(this);
    return view;
  }
}

/** Represents one band with events. */
class Events {
  /**
   * Assigns events for this band. Events with type between |eventTypeMin| and
   * |eventTypeMax| are only displayed on the band.
   *
   * @param {Object[]} events non-filtered list of all events. Each has array
   *     where first element is type and second is timestamp.
   * @param {number} eventTypeMin minimum inclusive type of the event to be
   *     displayed on this band.
   * @param {number} eventTypeMax maximum inclusive type of the event to be
   *     displayed on this band.
   */
  constructor(events, eventTypeMin, eventTypeMax) {
    this.events = events;
    this.eventTypeMin = eventTypeMin;
    this.eventTypeMax = eventTypeMax;
  }

  /**
   * Helper that finds next or previous event. Events that pass filter are
   * only processed.
   *
   * @param {number} index starting index for the search, not inclusive.
   * @param {direction} direction to search, 1 means to find the next event and
   *     -1 means the previous event.
   * @returns {number} index of the next or previous event or -1 in case not
   *     found.
   */
  getNextEvent(index, direction) {
    while (true) {
      index += direction;
      if (index < 0 || index >= this.events.length) {
        return -1;
      }
      if (this.events[index][0] >= this.eventTypeMin &&
          this.events[index][0] <= this.eventTypeMax) {
        return index;
      }
    }
  }

  /**
   * Helper that finds first event. Events that pass filter are only processed.
   *
   * @returns {number} index of the first event or -1 in case not found.
   */
  getFirstEvent() {
    return this.getNextEvent(-1 /* index */, 1 /* direction */);
  }

  /**
   * Helper that returns render attributes for the event.
   *
   * @param {number} index element index in |this.events|.
   */
  getEventAttributes(index) {
    return eventAttributes[this.events[index][0]];
  }

  /**
   * Returns true if the tested event denotes end of event sequence.
   *
   * @param {number} index element index in |this.events|.
   */
  isEndOfSequence(index) {
    var nextEventTypes = endSequenceEvents[this.events[index][0]];
    if (!nextEventTypes) {
      return false;
    }
    if (nextEventTypes.length == 0) {
      return true;
    }
    var nextIndex = this.getNextEvent(index, 1 /* direction */);
    if (nextIndex < 0) {
      // No more events after and it is listed as possible end of sequence.
      return true;
    }
    return nextEventTypes.includes(this.events[nextIndex][0]);
  }

  /**
   * Returns the index of closest event to the requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getClosest(timestamp) {
    if (this.events.length == 0) {
      return -1;
    }
    if (this.events[0][1] >= timestamp) {
      return this.getFirstEvent();
    }
    if (this.events[this.events.length - 1][1] <= timestamp) {
      return this.getNextEvent(
          this.events.length /* index */, -1 /* direction */);
    }
    // At this moment |firstBefore| and |firstAfter| points to any event.
    var firstBefore = 0;
    var firstAfter = this.events.length - 1;
    while (firstBefore + 1 != firstAfter) {
      var candidateIndex = Math.ceil((firstBefore + firstAfter) / 2);
      if (this.events[candidateIndex][1] < timestamp) {
        firstBefore = candidateIndex;
      } else {
        firstAfter = candidateIndex;
      }
    }
    // Point |firstBefore| and |firstAfter| to the supported event types.
    firstBefore =
        this.getNextEvent(firstBefore + 1 /* index */, -1 /* direction */);
    firstAfter =
        this.getNextEvent(firstAfter - 1 /* index */, 1 /* direction */);
    if (firstBefore < 0) {
      return firstAfter;
    } else if (firstAfter < 0) {
      return firstBefore;
    } else {
      var diffBefore = timestamp - this.events[firstBefore][1];
      var diffAfter = this.events[firstAfter][1] - timestamp;
      if (diffBefore < diffAfter) {
        return firstBefore;
      } else {
        return firstAfter;
      }
    }
  }

  /**
   * Returns the index of the first event after or on requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getFirstAfter(timestamp) {
    var closest = this.getClosest(timestamp);
    if (closest < 0) {
      return -1;
    }
    if (this.events[closest][1] >= timestamp) {
      return closest;
    }
    return this.getNextEvent(closest, 1 /* direction */);
  }

  /**
   * Returns the index of the last event before or on requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getLastBefore(timestamp) {
    var closest = this.getClosest(timestamp);
    if (closest < 0) {
      return -1;
    }
    if (this.events[closest][1] <= timestamp) {
      return closest;
    }
    return this.getNextEvent(closest, -1 /* direction */);
  }
}

/**
 * Creates visual representation of graphic buffers event model.
 *
 * @param {Object} model object produced by |ArcTracingGraphicsModel|.
 */
function setGraphicBuffersModel(model) {
  // Clear previous content.
  $('arc-event-bands').textContent = '';
  activeModel = model;

  // Microseconds per pixel. 100% zoom corresponds to 100 mcs per pixel.
  var resolution = zooms[zoomLevel];
  var parent = $('arc-event-bands');

  var topBandHeight = 16;
  var topBandPadding = 4;
  var innerBandHeight = 12;
  var innerBandPadding = 2;
  var innerLastBandPadding = 12;
  var chartHeight = 48;

  var vsyncEvents = new Events(
      model.android.global_events, 400 /* kVsync */, 400 /* kVsync */);

  var cpusTitle = new EventBandTitle(parent, 'CPUs', 'arc-events-band-title');
  var cpusBands = new CpuEventBands(
      cpusTitle, 'arc-events-band', resolution, 0, model.duration);
  cpusBands.setWidth(cpusBands.timestampToOffset(model.duration));
  cpusBands.setModel(model);
  cpusBands.addChartToExistingArea(0 /* top */, cpusBands.height);
  cpusBands.addChartSources(
      [new Events(model.system.memory, 8 /* kCpuTemp */, 8 /* kCpuTemp */)],
      true /* smooth */);
  cpusBands.setVSync(vsyncEvents);

  var memoryTitle =
      new EventBandTitle(parent, 'Memory', 'arc-events-band-title');
  var memoryBands = new EventBands(
      memoryTitle, 'arc-events-band', resolution, 0, model.duration);
  memoryBands.setWidth(memoryBands.timestampToOffset(model.duration));
  memoryBands.addChart(chartHeight, topBandPadding);
  // Used memory chart.
  memoryBands.addChartSources(
      [new Events(model.system.memory, 1 /* kMemUsed */, 1 /* kMemUsed */)],
      true /* smooth */);
  // Swap memory chart.
  memoryBands.addChartSources(
      [
        new Events(model.system.memory, 2 /* kSwapRead */, 2 /* kSwapRead */),
        new Events(
            model.system.memory, 3 /* kSwapWrite */, 3 /* kSwapWrite */)
      ],
      true /* smooth */);
  // Geom objects and size.
  memoryBands.addChartSources(
      [new Events(
          model.system.memory, 5 /* kGemObjects */, 5 /* kGemObjects */)],
      true /* smooth */);
  memoryBands.addChartSources(
      [new Events(model.system.memory, 6 /* kGemSize */, 6 /* kGemSize */)],
      true /* smooth */);
  memoryBands.setVSync(vsyncEvents);

  var chromeTitle =
      new EventBandTitle(parent, 'Chrome graphics', 'arc-events-band-title');
  var chromeBands = new EventBands(
      chromeTitle, 'arc-events-band', resolution, 0, model.duration);
  chromeBands.setWidth(chromeBands.timestampToOffset(model.duration));
  for (var i = 0; i < model.chrome.buffers.length; i++) {
    chromeBands.addBand(
        new Events(model.chrome.buffers[i], 500, 599), topBandHeight,
        topBandPadding);
  }

  chromeBands.setVSync(vsyncEvents);
  var chromeJanks = new Events(
      model.chrome.global_events, 505 /* kChromeOSJank */,
      505 /* kChromeOSJank */);
  chromeBands.addGlobal(chromeJanks);

  chromeBands.addChartToExistingArea(0 /* top */, chromeBands.height);
  chromeBands.addChartSources(
      [new Events(model.system.memory, 7 /* kGpuFreq */, 7 /* kGpuFreq */)],
      false /* smooth */);


  var androidTitle =
      new EventBandTitle(parent, 'Android graphics', 'arc-events-band-title');
  var androidBands = new EventBands(
      androidTitle, 'arc-events-band', resolution, 0, model.duration);
  androidBands.setWidth(androidBands.timestampToOffset(model.duration));
  androidBands.addBand(
      new Events(model.android.buffers[0], 400, 499), topBandHeight,
      topBandPadding);
  // Add vsync events
  androidBands.setVSync(vsyncEvents);
  var androidJanks = new Events(
      model.android.global_events, 405 /* kSurfaceFlingerCompositionJank */,
      405 /* kSurfaceFlingerCompositionJank */);
  androidBands.addGlobal(androidJanks);

  var allActivityJanks = [];
  var allActivityCustomEvents = [];
  for (var i = 0; i < model.views.length; i++) {
    var view = model.views[i];
    var activityTitleText;
    var icon;
    if (model.tasks && view.task_id in model.tasks) {
      activityTitleText =
          model.tasks[view.task_id].title + ' - ' + view.activity;
      icon = model.tasks[view.task_id].icon;
    } else {
      activityTitleText = 'Task #' + view.task_id + ' - ' + view.activity;
    }
    var activityTitle = new EventBandTitle(
        parent, activityTitleText, 'arc-events-band-title', icon);
    var activityBands = new EventBands(
        activityTitle, 'arc-events-band', resolution, 0, model.duration);
    activityBands.setWidth(activityBands.timestampToOffset(model.duration));
    for (var j = 0; j < view.buffers.length; j++) {
      // Android buffer events.
      activityBands.addBand(
          new Events(view.buffers[j], 100, 199), innerBandHeight,
          innerBandPadding);
      // exo events.
      activityBands.addBand(
          new Events(view.buffers[j], 200, 299), innerBandHeight,
          innerBandPadding /* padding */);
      // Chrome buffer events are not displayed at this time.

      // Add separator between buffers.
      if (j != view.buffers.length - 1) {
        activityBands.addBandSeparator(innerBandPadding);
      }
    }
    // Add vsync events
    activityBands.setVSync(vsyncEvents);

    var activityJank = new Events(
        view.global_events, 106 /* kBufferFillJank */,
        106 /* kBufferFillJank */);
    activityBands.addGlobal(activityJank);
    allActivityJanks.push(activityJank);

    var activityCustomEvents = new Events(
        view.global_events, 600 /* kCustomEvent */, 600 /* kCustomEvent */);
    activityBands.addGlobal(activityCustomEvents);
    allActivityCustomEvents.push(activityCustomEvents);
  }

  // Input section if exists.
  if (model.input && model.input.buffers.length > 0) {
    var inputTitle =
        new EventBandTitle(parent, 'Input', 'arc-events-band-title');
    var inputBands = new EventBands(
        inputTitle, 'arc-events-band', resolution, 0, model.duration);
    inputBands.setWidth(inputBands.timestampToOffset(model.duration));
    for (var i = 0; i < model.input.buffers.length; i++) {
      inputBands.addBand(
          new Events(model.input.buffers[i], 700, 799), topBandHeight,
          topBandPadding);
    }
    inputBands.setVSync(vsyncEvents);
  }

  // Create time ruler.
  var timeRulerEventHeight = 16;
  var timeRulerLabelHeight = 92;
  var timeRulerTitle =
      new EventBandTitle(parent, '' /* title */, 'arc-time-ruler-title');
  var timeRulerBands = new EventBands(
      timeRulerTitle, 'arc-events-band', resolution, 0, model.duration);
  timeRulerBands.setWidth(timeRulerBands.timestampToOffset(model.duration));
  // Reseve space for ticks and global events.
  timeRulerBands.updateHeight(timeRulerEventHeight, 0 /* padding */);

  var kTimeMark = 10000;
  var kTimeMarkSmall = 10001;
  var timeEvents = [];
  var timeTick = 0;
  var timeTickOffset = 20 * resolution;
  var timeTickIndex = 0;
  while (timeTick < model.duration) {
    if ((timeTickIndex % 10) == 0) {
      timeEvents.push([kTimeMark, timeTick]);
    } else {
      timeEvents.push([kTimeMarkSmall, timeTick]);
    }
    timeTick += timeTickOffset;
    ++timeTickIndex;
  }
  var timeMarkEvents = new Events(timeEvents, kTimeMark, kTimeMarkSmall);
  timeRulerBands.addGlobal(timeMarkEvents);

  // Add all janks
  timeRulerBands.addGlobal(chromeJanks, 'circle' /* renderType */);
  timeRulerBands.addGlobal(androidJanks, 'circle' /* renderType */);
  for (var i = 0; i < allActivityJanks.length; ++i) {
    timeRulerBands.addGlobal(allActivityJanks[i], 'circle' /* renderType */);
  }
  for (var i = 0; i < allActivityCustomEvents.length; ++i) {
    timeRulerBands.addGlobal(
        allActivityCustomEvents[i], 'circle' /* renderType */);
  }
  // Add vsync events
  timeRulerBands.setVSync(vsyncEvents);

  // Reseve space for labels.
  // Add tick labels.
  timeRulerBands.updateHeight(timeRulerLabelHeight, 0 /* padding */);
  timeTick = 0;
  timeTickOffset = 200 * resolution;
  while (timeTick < model.duration) {
    SVG.addText(
        timeRulerBands.svg, timeRulerBands.timestampToOffset(timeTick),
        timeRulerEventHeight, timeRulerBands.fontSize,
        timestampToMsText(timeTick));
    timeTick += timeTickOffset;
  }
  // Add janks and custom events labels.
  var rotationY = timeRulerEventHeight + timeRulerBands.fontSize;
  for (var i = 0; i < timeRulerBands.globalEvents.length; ++i) {
    var globalEvents = timeRulerBands.globalEvents[i];
    if (globalEvents == timeMarkEvents ||
        globalEvents == timeRulerBands.vsyncEvents) {
      continue;
    }
    var index = globalEvents.getFirstEvent();
    while (index >= 0) {
      var event = globalEvents.events[index];
      index = globalEvents.getNextEvent(index, 1 /* direction */);
      var eventType = event[0];
      var attributes = eventAttributes[eventType];
      var text;
      if (eventType == 600 /* kCustomEvent */) {
        text = event[2];
      } else {
        text = attributes.name;
      }
      var x =
          timeRulerBands.timestampToOffset(event[1]) - timeRulerBands.fontSize;
      SVG.addText(
          timeRulerBands.svg, x, timeRulerEventHeight, timeRulerBands.fontSize,
          text, 'start' /* anchor */,
          'rotate(45 ' + x + ', ' + rotationY + ')' /* transform */);
    }
  }

  $('arc-graphics-tracing-save').disabled = false;
}
