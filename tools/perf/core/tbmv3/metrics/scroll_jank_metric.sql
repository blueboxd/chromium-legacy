-- Copyright 2020 The Chromium Authors. All rights reserved.
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- The metric finds periods in a trace when scroll update events started
-- but didn't finish. The idea is that it's perceived by the user as a scroll
-- delay, i.e. jank.
-- Note that there's a similar metric in janky_scroll_periods.sql that uses
-- a slightly different approach to estimate jankiness. Keeping two metrics
-- around is a temporary measure for experimental purposes. In the end we want
-- to leave only one of them.
-- TODO(khokhlov): Remove one of these metrics.

-- This selects GestureScrollUpdate events.
CREATE VIEW input_latency_events AS
SELECT
  ts AS start, ts + dur AS finish
FROM slice
WHERE
  name = "InputLatency::GestureScrollUpdate";

-- Every row of this query is a span from one event finish to the next.
-- Column names 'ts' and 'dur' are as expected by SPAN_JOIN function.
CREATE VIEW intervals_between_finishes AS
SELECT
  finish AS ts,
  LEAD(finish) OVER (ORDER BY finish) AS next_ts,
  LEAD(finish) OVER (ORDER BY finish) - finish AS dur
FROM input_latency_events;

-- Every row of this query marks the start of some GestureScrollUpdate event.
-- Column names 'ts' and 'dur' are as expected by SPAN_JOIN function.
CREATE VIEW start_moments AS
SELECT start AS ts, 0 AS dur
FROM input_latency_events
ORDER BY ts;

-- This span-joins the two views and yields a table where every start moment
-- is joined with the corresponding span between finishes.
CREATE VIRTUAL TABLE starts_joined
USING SPAN_JOIN(intervals_between_finishes, start_moments);

-- Now we can count how many starts there were between every two finishes
-- and how long it took from the earliest start to the finish.
CREATE VIEW janks AS
SELECT next_ts - MIN(ts) AS dur, COUNT(*) AS n_starts
FROM starts_joined
GROUP BY next_ts;

-- We can use different thresholds for calling interval a jank.
-- Here we set two thresholds of 17ms and 34ms, that correspond to the duration
-- of 1 and 2 frames at 60 fps, rounded up.
-- TODO(khokhlov): Investigate how this metric handles breaks between scrolls
-- and dropped frames.
CREATE VIEW scroll_jank_metric AS
SELECT
  (SELECT COUNT(*) FROM janks WHERE dur > 17e6 AND n_starts > 1) AS num_janks,
  (SELECT COUNT(*) FROM janks WHERE dur > 34e6 AND n_starts > 1) AS num_big_janks,
  (SELECT COALESCE(SUM(dur) / 1e6, 0) FROM janks WHERE dur > 17e6 AND n_starts > 1) AS total_jank_duration;

-- Output the proto with metric values.
CREATE VIEW scroll_jank_metric_output AS
SELECT ScrollJankMetric(
  'num_janks', num_janks,
  'num_big_janks', num_big_janks,
  'total_jank_duration', total_jank_duration)
FROM scroll_jank_metric;
