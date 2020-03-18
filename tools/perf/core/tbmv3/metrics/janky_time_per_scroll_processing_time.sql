--
-- A collection of metrics related to GestureScroll events.
--
-- We compute a couple different metrics.
--
-- 1) GestureScrollMilliseconds
--      The time spent scrolling during the trace. Measured as the timestamp of
--      the GestureScrollBegin to the end timestamp of the last event (either
--      GestureScrollEnd or a GestureScrollUpdate that extends past the
--      GestureScrollEnd).
-- 2) GestureScrollProcessingMilliseconds
--      The time spent in milliseconds working on GestureScrollUpdates. This
--      differs from "GestureScrollMilliseconds" by including overlapping
--      events. For example if a scroll is made of two events from microsecond 1
--      to microsecond 4 like this:
--        0  1  2  3
--        |-----|
--           |-----|
--      Then GestureScrollMilliseconds will by 3 (scroll started at 0 and ended
--      at 3), and Processing time will be 4 (both events are duration 2 so the
--      sum is 4).
-- 3) Jank.NumJankyGestureScrollUpdates
--      The number of GestureScrollUpdate events we believe can be classified as
--      "janky". Definition of janky is given below.
-- 4) Jank.JankyGestureScrollMilliseconds
--      The sum of the duration of all the events classified as "janky". I.E.
--      for every event included in the count of
--      |Jank.NumJankyGestureScrollUpdates| sum their duration.
-- 5) Jank.JankyTimePerScrollProcessingTimePercentage
--      This is |Jank.JankyGestureScrollMilliseconds| divided by
--      |GestureScrollProcessingMilliseconds| and then multiplied into a
--      percentage. Due to limitations this is rounded down to the nearest whole
--      percentage.
--
-- We define a GestureScrollUpdate to be janky if comparing forwards or
-- backwards (ignoring coalesced updates) a given GestureScrollUpdate exceeds
-- the duration of its predecessor or successor by 50% of a vsync interval
-- (currently pegged to 60 FPS).
--
-- WARNING: This metric should not be used as a source of truth. It is under
--          active development and the values & meaning might change without
--          notice.


-- Get all the GestureScrollBegin and GestureScrollEnd events. We take their
-- IDs to group them together into scrolls later and the timestamp and duration
-- to compute the duration of the scroll.

DROP VIEW IF EXISTS ScrollBeginsAndEnds;

CREATE VIEW ScrollBeginsAndEnds AS
  SELECT
    ROW_NUMBER() OVER (ORDER BY ts ASC) AS rowNumber,
    name,
    id AS scrollId,
    ts AS scrollTs,
    dur AS scrollDur
  FROM
    slice
  WHERE
    name IN (
      'InputLatency::GestureScrollBegin',
      'InputLatency::GestureScrollEnd'
    )
  ORDER BY ts ASC;

-- Now we take the Begin and the End events and join the information into a
-- single row per scroll.

DROP VIEW IF EXISTS JoinedScrollBeginsAndEnds;

CREATE VIEW JoinedScrollBeginsAndEnds AS
  SELECT
    begin.scrollId AS beginId,
    begin.scrollTs AS scrollBegin,
    end.scrollTs + end.scrollDur AS maybeScrollEnd
  FROM ScrollBeginsAndEnds begin JOIN ScrollBeginsAndEnds end ON
    begin.rowNumber + 1 = end.rowNumber AND
    begin.name = 'InputLatency::GestureScrollBegin' AND
    end.name = 'InputLatency::GestureScrollEnd';

-- Get the GestureScrollUpdate events by name ordered by timestamp, compute
-- the number of frames (relative to 60 fps) that each event took. 1.6e+7 is
-- 16 ms in nanoseconds. We also each GestureScrollUpdate event to the
-- information about it's begin and end for easy computation later.
--
-- We remove updates with |dur| == -1 because this means we have no end event
-- and can't reasonably determine what it should be. We have separate tracking
-- to ensure this only happens at the end of the trace.

DROP VIEW IF EXISTS GestureScrollUpdates;

CREATE VIEW GestureScrollUpdates AS
SELECT
  ROW_NUMBER() OVER (ORDER BY ts ASC) AS rowNumber,
  beginId,
  scrollBegin,
  CASE WHEN
    maybeScrollEnd > ts + dur THEN
      maybeScrollEnd ELSE
      ts + dur
    END as scrollEnd,
  ts as scrollTs,
  dur as updateDur,
  id AS scrollId,
  arg_set_id AS scrollArgSetId,
  dur AS scrollDur,
-- TODO(nuskos): Replace 1.6e.7 with a sub query that computes the vsync
--               interval for this scroll.
  dur/1.6e+7 AS scrollFramesExact
FROM JoinedScrollBeginsAndEnds beginAndEnd LEFT JOIN (
  SELECT
    *
  FROM
    slice
  WHERE
    name = 'InputLatency::GestureScrollUpdate' AND
    dur != -1 AND
-- TODO(nuskos): Currently removing updates with dur < 1.5e+7 (15 ms i.e. less
--               then a vsync interval at 60 fps) is a hack to work around the
--               fact we don't have typed async events yet. Once we can tell if
--               an update is coalesced or not we should remove this and instead
--               check that directly.
    dur >= 1.5e+7
) scrollUpdate ON
  scrollUpdate.ts < beginAndEnd.MaybeScrollEnd AND
  scrollUpdate.ts >= beginAndEnd.ScrollBegin
ORDER BY ts ASC;

-- This takes the GestureScrollUpdate and joins it to the previous row (NULL
-- if there isn't one) and the next row (NULL if there isn't one). And then
-- computes if the duration of the event (relative to 60 fps) increased by more
-- then 0.5 (which is 1/2 of 16 ms and hopefully eventually replaced with vsync
-- interval).
--
-- We only compare a ScrollUpdate within its scroll
-- (currBeginId == prev/next BeginId). This controls somewhat for variability
-- of scrolls.

DROP VIEW IF EXISTS ScrollJanksMaybeNull;

CREATE VIEW ScrollJanksMaybeNull AS
  SELECT
    ROW_NUMBER() OVER (ORDER BY currScrollTs ASC) AS rowNumber,
    currBeginId,
    currUpdateDur,
    currScrollDur,
    currScrollId,
    currScrollTs,
    CASE WHEN currBeginId != prevBeginId
    THEN
      0 ELSE
      currScrollFramesExact > prevScrollFramesExact + 0.5
    END AS prevJank,
    CASE WHEN currBeginId != next.BeginId
    THEN
      0 ELSE
      currScrollFramesExact > next.ScrollFramesExact + 0.5
    END AS nextJank,
    prevScrollFramesExact,
    currScrollFramesExact,
    next.ScrollFramesExact as nextScrollFramesExact
  FROM (
   SELECT
     curr.beginId as currBeginId,
     curr.updateDur as currUpdateDur,
     curr.scrollEnd - curr.ScrollBegin as currScrollDur,
     curr.rowNumber AS currRowNumber,
     curr.scrollId AS currScrollId,
     curr.scrollTs as currScrollTs,
     curr.scrollFramesExact AS currScrollFramesExact,
     prev.beginId as prevBeginId,
     prev.scrollFramesExact AS prevScrollFramesExact
   FROM
     GestureScrollUpdates curr LEFT JOIN
     GestureScrollUpdates prev ON prev.rowNumber + 1 = curr.rowNumber
   ) currprev JOIN
   GestureScrollUpdates next ON currprev.currRowNumber + 1 = next.rowNumber
ORDER BY currprev.currScrollTs ASC;

-- This just lists outs the rowNumber (which is ordered by timestamp), its jank
-- status and information about the update and scroll overall. Basically
-- getting it into a next queriable format.

DROP VIEW IF EXISTS ScrollJanks;

CREATE VIEW ScrollJanks AS
  SELECT
    rowNumber,
    currBeginId,
    currUpdateDur as currUpdateDur,
    currScrollDur,
    (nextJank IS NOT NULL AND nextJank) OR
    (prevJank IS NOT NULL AND prevJank)
    AS jank
  FROM ScrollJanksMaybeNull;

-- Compute the total amount of nanoseconds from Janky GestureScrollUpdates and
-- the total amount of nanoseconds we spent scrolling in the trace. Also need
-- to get the total time on all GestureScrollUpdates. We cast them to float to
-- ensure our devision results in a percentage and not just integer division
-- equal to 0.
--
-- We need to select MAX(currScrollDur) because the last few
-- GestureScrollUpdates might extend past the GestureScrollEnd event so we
-- select the number which is the last part of the scroll.
--
-- TODO(nuskos): We should support more types (floats and strings) in our
--               metrics as well as support for specifying units (nanoseconds).

DROP VIEW IF EXISTS JankyNanosPerScrollNanosMaybeNull;

CREATE VIEW JankyNanosPerScrollNanosMaybeNull AS
  SELECT
    SUM(jank) as numJankyUpdates,
    SUM(CASE WHEN jank = 1 THEN
      currUpdateDur ELSE
      0 END) as jankyNanos,
    SUM(currUpdateDur) AS totalProcessingNanos,
    (
      SELECT sum(scrollDur)
      FROM (
        SELECT
          MAX(currScrollDur) AS scrollDur
        FROM ScrollJanks
        GROUP BY currBeginId
      )
    ) AS scrollNanos
  FROM ScrollJanks;

DROP VIEW IF EXISTS janky_time_per_scroll_processing_time;

CREATE VIEW janky_time_per_scroll_processing_time AS
  SELECT
    (SELECT
      CASE WHEN
        totalProcessingNanos IS NULL OR jankyNanos IS NULL OR
        totalProcessingNanos = 0 THEN
          0.0 ELSE
          (CAST(jankyNanos AS FLOAT) / CAST(totalProcessingNanos AS FLOAT)) * 100.0
        END
    FROM JankyNanosPerScrollNanosMaybeNull) AS jankyPercentage,
    (SELECT
      COALESCE(scrollNanos, 0) / 1000000
    FROM JankyNanosPerScrollNanosMaybeNull) AS scrollMillis,
    (SELECT
      COALESCE(jankyNanos, 0) / 1000000
    FROM JankyNanosPerScrollNanosMaybeNull) AS jankyMillis,
    (SELECT
      COALESCE(totalProcessingNanos, 0) / 1000000
    FROM JankyNanosPerScrollNanosMaybeNull) AS processingMillis,
    (SELECT
      COALESCE(numJankyUpdates, 0)
    FROM JankyNanosPerScrollNanosMaybeNull) AS numJankyUpdates;

-- Specify how to fill the metrics proto properly.

DROP VIEW IF EXISTS janky_time_per_scroll_processing_time_output;

CREATE VIEW janky_time_per_scroll_processing_time_output AS
  SELECT JankyTimePerScrollProcessingTime(
    'janky_time_per_scroll_processing_time_percentage', jankyPercentage,
    'gesture_scroll_milliseconds', CAST(scrollMillis AS INT),
    'janky_gesture_scroll_milliseconds', jankyMillis,
    'gesture_scroll_processing_milliseconds', processingMillis,
    'num_janky_gesture_scroll_updates', numJankyUpdates)
  FROM janky_time_per_scroll_processing_time;
