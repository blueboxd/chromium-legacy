Definitions of LUCI entities that only exist for the master branch.

* **consoles**
  * manually curated consoles for chromium subproject builders
* **ci.star**
  * builders that do post-submit testing against the master branch
  * when new milestones are created, milestone-specific versions of the builders
  will not be created
* **try.star**, **gpu.try.star**, **swangle.try.star**
  * builders that do pre-submit testing against the master branch
  * when new milestones are created, milestone-specific versions of the builders
  will not be created
