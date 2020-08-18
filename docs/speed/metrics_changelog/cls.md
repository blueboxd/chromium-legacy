# Cumulative Layout Shift Changelog

This is a list of changes to [Cumulative Layout Shift](https://web.dev/cls).

* Chrome 86
  * Fixed bugs about ink overflows (crbug.com/1108622) and transforms (crbug.com/1109053).
  * Now we aggregate layout shift reports of:
    * an element and its descendants if they move together, and
    * inline elements and texts in a block after a shifted text.
  These changes will affect layout instability score for the specific cases.
* Chrome 79
  * Metric is elevated to stable; changes in metric definition will be reported in this log.
* Chrome 77
  * Metric exposed via API: [Cumulative Layout Shift](https://web.dev/cls/) available via [Layout Instability API](https://github.com/WICG/layout-instability)
