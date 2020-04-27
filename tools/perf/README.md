<!-- Copyright 2019 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Chrome Benchmarking System

This directory contains benchmarks and infrastructure to test Chrome and
Chromium and output performance measurements. The benchmarks are run on the
[perf waterfall](https://ci.chromium.org/p/chrome/g/chrome.perf/console). Most
of the benchmarks are run using
[Telemetry](https://chromium.googlesource.com/catapult.git/+/HEAD/telemetry/README.md),
which is a framework for driving Chrome through test cases ("stories").

Telemetry often makes use of
[Web Page Replay (WPR)](https://chromium.googlesource.com/catapult.git/+/HEAD/web_page_replay_go/README.md)
to save a version of a website so that we can measure performance without fear
of the website changing underneath us.

For doing measurements, Telemetry usually makes use of an event tracing system
built into Chromium. Event data is tracked as Chromium runs. After Chromium
finishes running that event data is processed by either
[TBMv2](https://chromium.googlesource.com/catapult.git/+/HEAD/tracing/tracing/metrics)
(which is the current system) or
[TBMv3](https://chromium.googlesource.com/chromium/src/+/HEAD/tools/perf/core/tbmv3)
(which is an experimental new system) to create measurements.

Those measurements are uploaded to the
[ChromePerf Dashboard](https://chromeperf.appspot.com/) which charts
measurements over time and alerts on regressions. Regressions can be bisected
using [Pinpoint](https://pinpoint-dot-chromeperf.appspot.com/) to figure out
which Chromium change caused them.

Pinpoint can also be used to run try jobs against machines in our data centers.
Information about available platforms for testing is available to Googlers at
[Chrome Benchmarking Sheet](https://goto.google.com/chrome-benchmarking-sheet).

Please also read the [Chrome Speed][speed] documentation to learn more about the
team organization and, in particular, the top level view of
[How Chrome Measures Performance][chrome_perf_how].

# Performance tools

This directory contains a variety of tools that can be used to run benchmarks,
interact with speed services, and manage performance waterfall configurations.
It also has commands for running functional unittests.

[speed]: /docs/speed/README.md
[chrome_perf_how]: /docs/speed/how_does_chrome_measure_performance.md

## run_tests

This command allows you to run functional tests against the python code in this
directory. For example, try:

```
./run_tests results_dashboard_unittest
```

Note that the positional argument can be any substring within the test name.

This may require you to set up your `gsutil config` first.

## run_benchmark

This command allows running benchmarks defined in the chromium repository,
specifically in [tools/perf/benchmarks][benchmarks_dir]. If you need it,
documentation is available on how to [run benchmarks locally][run_locally] and
how to properly [set up your device][device_setup].

[benchmarks_dir]: https://cs.chromium.org/chromium/src/tools/perf/benchmarks/
[run_locally]: https://chromium.googlesource.com/catapult.git/+/HEAD/telemetry/docs/run_benchmarks_locally.md
[device_setup]: /docs/speed/benchmark/telemetry_device_setup.md

## update_wpr

A helper script to automate various tasks related to the update of
[Web Page Recordings][wpr] for our benchmarks. In can help creating new
recordings from live websites, replay those to make sure they work, upload them
to cloud storage, and finally send a CL to review with the new recordings.

[wpr]: https://github.com/catapult-project/catapult/tree/master/web_page_replay_go

## pinpoint_cli

A command line interface to the [pinpoint][] service. Allows to create new jobs,
check the status of jobs, and fetch their measurements as csv files.

[pinpoint]: https://pinpoint-dot-chromeperf.appspot.com

## flakiness_cli

A command line interface to the [flakiness dashboard][].

[flakiness dashboard]: https://test-results.appspot.com/dashboards/flakiness_dashboard.html

## soundwave

Allows to fetch data from the [Chrome Performance Dashboard][chromeperf] and
stores it locally on a SQLite database for further analysis and processing. It
also allows defining [studies][], pre-sets of measurements a team is interested
in tracking, and uploads them to cloud storage to visualize with the help of
[Data Studio][]. This currently backs the [v8][v8_dashboard] and
[health][health_dashboard] dashboards.

[chromeperf]: https://chromeperf.appspot.com/
[studies]: https://cs.chromium.org/chromium/src/tools/perf/cli_tools/soundwave/studies/
[Data Studio]: https://datastudio.google.com/
[v8_dashboard]: https://datastudio.google.com/s/iNcXppkP3DI
[health_dashboard]: https://datastudio.google.com/s/jUXfKZXXfT8

## pinboard

Allows scheduling daily [pinpoint][] jobs to compare measurements with/without a
patch being applied. This is useful for teams developing a new feature behind a
flag, who wants to track the effects on performance as the development of their
feature progresses. Processed data for relevant measurements is uploaded to
cloud storage, where it can be read by [Data Studio][]. This also backs data
displayed on the [v8][v8_dashboard] dashboard.
