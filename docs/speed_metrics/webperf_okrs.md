# Web Performance Objectives

[TOC]

## 2020 Q2

### New web performance APIs

  * Work towards shipping
    **[performance.measureMemory](https://github.com/WICG/performance-measure-memory)**.
    This API intends to provide memory measurements for web pages without
    leaking information. It will replace the non-standard performance.memory and
    provide more accurate information, but will require the website to be
    [cross-origin
    isolated](https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/crossOriginIsolated).
    Try it out with the Origin Trial
    [here](https://web.dev/monitor-total-page-memory-usage/#using-performance.measurememory())!
    Deliverables for this quarter:
    * Extend the scope of the API to workers.
    * Draft a spec.

  * Work towards web perf support for **Single Page Apps** (SPAs). SPAs have
    long been mistreated by our web performance APIs, which mostly focus on the
    initial page load for ‘multi-page apps’. It will be a long process to
    resolve all measurement gaps, but we intend to start making progress on
    better performance measurements for SPAs by using a data-driven approach.
    Deliverables for this quarter:
    * Implement a strategy for measuring the performance of SPA navigations in
      RUM, based on explicit navigation annotations via User Timing.
    * Partner with some frameworks to gather data using said strategy.
    * Socialize an explainer with our ideas.

  * Work towards web perf support for **page abandonment**. Currently, our APIs
    are blind to a class of users that decide to leave the website very early
    on, before the performance measurement framework of the website is set into
    place. This quarter, we plan to create and socialize a proposal about
    measuring early page abandonment.

  * Ship the full **[Event Timing](https://github.com/WICG/event-timing)** API.
    Currently, Chrome ships only ‘first-input’ to enable users to measure their
    [First Input Delay](https://web.dev/fid/). We intend to ship support for
    ‘event’ so that developers can track all slow events. Each entry will
    include a ‘target’ attribute to know which was the EventTarget. We’ll
    support a durationThreshold parameter in the observer to tweak the duration
    of events being observed. Finally, we’ll also have performance.eventCounts
    to enable computing estimated percentiles based on the data received.

  * Ship a **[Page Visibility](https://github.com/w3c/page-visibility/)**
    observer. Right now, the Page Visibility API allows registering an event
    listener for future changes in visibility, but any visibility states prior
    to that are missed. The solution to this is having an observer which enables
    ‘buffered’ entries, so a full history of the visibility states of the page
    is available. An alternative considered was having a boolean flag in the
    PerformanceEntry stating that the page was backgrounded before the entry was
    created, but there was overwhelming
    [support](https://lists.w3.org/Archives/Public/public-web-perf/2020Apr/0005.html)
    for the observer instead.

  * Provide support for two Facebook-driven APIs:
    [isInputPending](https://github.com/WICG/is-input-pending) and [JavaScript
    Self-Profiling](https://github.com/WICG/js-self-profiling). The
    **isInputPending** API enables developers to query whether the browser has
    received but not yet processed certain kinds of user inputs. This way, work
    can be scheduled on longer tasks while still enabling the task to stopped
    when higher priority work arises. The **JS Self-Profiling** API enables
    developers to collect JS profiles from real users, given a sampling rate and
    capacity. It enables measuring the performance impact of specific JS
    functions and finding hotspots in JS code.

### Existing web performance API improvements

* Ship the
  [sources](https://github.com/WICG/layout-instability#Source-Attribution)
  attribute for the
  **[LayoutInstability](https://github.com/WICG/layout-instability)** API. The
  Layout Instability API provides excellent information about content shifting
  on a website. This API is already shipped in Chrome. However, it’s often hard
  to figure out which content is shifting. This new attribute will inform
  developers about the shifting elements and their locations within the
  viewport.

* **[LargestContentfulPaint](https://github.com/WICG/largest-contentful-paint)**:
  gather data about LCP without excluding DOM nodes that were removed. The
  Largest Contentful Paint API exposes the largest image or text that is painted
  in the page. Currently, content removed from the website is also removed as a
  candidate for LCP. However, this negatively affects some websites, for
  instance those with certain types of image carousels. This quarter, we’ll
  gather data internally to determine whether we should start including removed
  DOM content. The API itself will not change for now.

* _(Stretch)_ Work on exposing the **‘final’ LargestContentfulPaint** candidate.
  Currently LCP just emits a new entry whenever a new candidate is found. This
  means that a developer has no way to know when LCP is ‘done’, which can happen
  early on if there is some relevant user input in the page. We could consider
  surfacing an entry to indicate that LCP computations are finished and
  including the final LCP value, when possible. There’s also an
  [idea](https://github.com/WICG/largest-contentful-paint/issues/43#issuecomment-608569132)
  to include some heuristics to get a higher quality signal regarding whether
  the LCP obtained seems ‘valid’. If we have time this quarter, we’d be happy to
  do some exploration on this.

* _(Stretch)_ **[ResourceTiming](https://github.com/w3c/resource-timing)**:
  outline a plan to fix the problem of TAO (Timing-Allow-Origin) being an opt-in
  for non-timing information such as transferSize. This may mean using a new
  header or relying on some of the upcoming new security primitives in the web.
  If we have time this quarter, we’d like to begin tackling this problem by
  socializing a concrete proposal for a fix.

### Interop and documentation

* **[Paint Timing](https://github.com/w3c/paint-timing)**: change the Chromium
  implementation so it passes [new web platform
  tests](https://wpt.fyi/results/paint-timing/fcp-only?label=experimental&label=master&aligned).
  These tests are based on the feedback from WebKit. They intend to ship First
  Contentful Paint in the near future!

* Improve the **documentation** of our APIs on MDN and web.dev. We have been
  busily shipping new web perf APIs, but some of the documentation of them has
  lagged behind. For instance, we’ll make sure that there’s MDN pages on all of
  the new APIs we’ve shipped, and we’ll collaborate with DevRel to ensure that
  the documentation on web.dev is accurate.
