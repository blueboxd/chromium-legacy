# Representative Performance Tests for Rendering Benchmark

`rendering_representative_perf_tests` runs a sub set of stories from rendering
benchmark on CQ, to prevent performance regressions. For each platform there is
a `story_tag` which describes the representative stories used in this test.
These stories will be tested using the [`run_benchmark`](../../tools/perf/run_benchmark) script. Then the recorded values for `frame_times` will be
compared with the historical upper limit described in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`](../../testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json).

[TOC]

## Clustering the Benchmark and Choosing Representatives

The clustering of the benchmark is based on the historical values recorded for
`frame_times`. For steps on clustering the benchmark check [Clustering benchmark stories](../../tools/perf/experimental/story_clustering/README.md).

Currently there are three sets of representatives described by story tags below:
*   `representative_mac_desktop`
*   `representative_mobile`
*   `representative_win_desktop`

Adding more stories to representatives or removing stories from the set is
managed by adding and removing story tags above to stories in [rendering benchmark](../../tools/perf/page_sets/rendering).

## Updating the Upper Limits

The upper limits for averages and confidence interval (CI) ranges of
`frame_times` described in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`](../../testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json)
are used to passing or failing a test. These values are the 95 percentile of
the past 30 runs of the test on each platform (for both average and CI).

This helps with catching sudden regressions which results in a value higher
than the upper limits. But in case of gradual regressions, the upper limits
may not be useful in not updated frequently. Updating these upper limits also
helps with adopting to improvements.

Updating these values can be done by running [`src/tools/perf/experimental/representative_perf_test_limit_adjuster/adjust_upper_limits.py`](../../tools/perf/experimental/representative_perf_test_limit_adjuster/adjust_upper_limits.py)and committing the changes.
The script will create a new JSON file using the values of recent runs in place
of [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`](../../testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json).

## Updating Expectations

To skip any of the tests, update
[`src/tools/perf/expectations.config`](../../tools/perf/expectations.config) and
add the story under rendering benchmark.