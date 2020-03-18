-- Copyright 2020 The Chromium Authors. All rights reserved.
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

-- A metric that counts the number of janky scroll periods. A janky scroll period
-- is defined as a time period during which:
--   (i)  Chrome was handling GuestureScrollUpdate input events, and
--   (ii) sequential GuestureScrollUpdate events had a duration that differed
--        by more than half a frame.
-- The metric assumes a frame rate of 60fps.
--
-- Note that there's a similar metric in scroll_jank_metric.sql that uses
-- a slightly different approach to estimate jankiness. Keeping two metrics
-- around is a temporary measure for experimental purposes. In the end we want
-- to leave only one of them.
-- TODO(khokhlov): Remove one of these metrics.

-- Get the GestureScrollUpdate events by name ordered by timestamp, compute
-- the number of frames (relative to 60 fps) that each event took. 1.6e+7 is
-- 16 ms in nanoseconds.
CREATE VIEW GestureScrollUpdates AS
SELECT
  ROW_NUMBER() OVER (ORDER BY ts ASC) AS rowNumber,
  id AS ScrollId,
  ts AS ScrollTs,
  arg_set_id AS ScrollArgSetId,
  dur AS ScrollDur,
  dur/1.6e+7 AS ScrollFramesExact,
  CAST(dur/1.6e+7 AS int) AS ScrollFramesRounded
FROM
  slice
WHERE
  name = 'InputLatency::GestureScrollUpdate'
ORDER BY ScrollTs;

-- This takes the GestureScrollUpdate and joins it to the previous row (NULL
-- if there isn't one) and the next row (NULL if there isn't one). And then
-- computes whether the duration of the event (relative to 60 fps) varied by
-- more than 0.5 (which is 1/2 of 16 ms).
CREATE VIEW ScrollJanksComplete AS
SELECT
  ROW_NUMBER() OVER (ORDER BY currTs ASC) AS rowNumber,
  currScrollId,
  currTs,
  currScrollFramesExact,
  currScrollFramesExact
    NOT BETWEEN
      prevScrollFramesExact - 0.5 AND
      prevScrollFramesExact + 0.5
    AS prevJank,
  currScrollFramesExact
    NOT BETWEEN
      next.ScrollFramesExact - 0.5 AND
      next.ScrollFramesExact + 0.5
    AS nextJank,
  (
    currScrollFramesExact
      NOT BETWEEN
        prevScrollFramesExact - 0.5 AND
        prevScrollFramesExact + 0.5
  ) AND (
    currScrollFramesExact
      NOT BETWEEN
        next.ScrollFramesExact - 0.5 AND
        next.ScrollFramesExact + 0.5
  ) AS bothJank
FROM (
    SELECT
      curr.rowNumber AS currRowNumber,
      curr.Scrollts AS currTs,
      curr.ScrollId AS currScrollId,
      curr.ScrollFramesExact AS currScrollFramesExact,
      prev.ScrollFramesExact AS prevScrollFramesExact
    FROM
      GestureScrollUpdates curr LEFT JOIN
      GestureScrollUpdates prev ON prev.rowNumber + 1 = curr.rowNumber
  ) currprev JOIN
  GestureScrollUpdates next ON currprev.currRowNumber + 1 = next.rowNumber
ORDER BY currprev.currTs ASC;

-- This just lists outs the rowNumber (which is ordered by timestamp) and
-- whether it was a janky slice (as defined by comparing to both the next and
-- previous slice).
CREATE VIEW ScrollJanks AS
  SELECT
    rowNumber,
    bothJank OR
    (nextJank AND prevJank IS NULL) OR
    (prevJank AND nextJank IS NULL)
    AS Jank
  FROM ScrollJanksComplete;

-- This sums the number of periods with sequential janky slices. When Chrome
-- experiences a jank it often stumbles for a while, this attempts to
-- encapsulate this by only counting thestumbles and not all the janky
-- individual slices.
CREATE VIEW JankyScrollPeriodsView AS
  SELECT
    SUM(CASE WHEN curr.Jank = 1 THEN 1 ELSE 0 END) as gestures,
    SUM(CASE WHEN curr.Jank = 1 AND (prev.Jank IS NULL OR prev.Jank = 0)
        THEN 1 ELSE 0 END) AS periods
  FROM (
    SELECT * from ScrollJanks
  ) curr LEFT JOIN (
    SELECT * from ScrollJanks
  ) prev ON curr.rowNumber = prev.rowNumber + 1;

-- Output the proto with metric values.
CREATE VIEW janky_scroll_periods_output AS
SELECT JankyScrollPeriods(
    'num_gestures', COALESCE(gestures, 0),
    'num_periods', COALESCE(periods, 0))
FROM JankyScrollPeriodsView;
