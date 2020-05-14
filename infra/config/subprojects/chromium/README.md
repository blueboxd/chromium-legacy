Definitions of LUCI entities that test the chromium/src codebase.

* **master-only**
  * builders not on the main waterfall that are only run against the master
  branch
  * milestone-specific versions of these builders do not exist
* **versioned**
  * builders on the main waterfall for milestones
  * milestones are being switched to use separate projects, so this is only used
  for milestones <= M83.
* **ci.star**
  * main waterfall builders that do post-submit testing against the master
  branch
  * when new milestones are created, milestone-specific versions of the builders
  will be created
* **try.star**, **gpu.try.star**, **swangle.try.star**
  * main waterfall builders that do pre-submit testing against the master branch
  * when new milestones are created, milestone-specific versions of the builders
  will be created
