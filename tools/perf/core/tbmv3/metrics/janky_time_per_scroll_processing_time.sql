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

-- Get all processes which are related to Chrome to the best of our current
-- ability.
DROP VIEW IF EXISTS ChromeProcessesInfo;

CREATE VIEW ChromeProcessesInfo AS
  SELECT
    *
  FROM process
  WHERE name IS NOT NULL AND (
    name = "Browser" OR
    name = "Renderer" OR
    name = "Gpu" OR
    name = "GPU Process" OR
    name LIKE "com%chrome%"
  );

-- When working on GestureScrollUpdate events we need to ensure we have all the
-- events from the browser, renderer, and GPU processes. This query isn't quite
-- perfect. In system tracing we could have 3 browser processes all in the
-- background and this would match, but for now its the best we can do, and
-- should filter 99% (citation needed) of what we want.
--
-- See b/151077536 for historical context.
DROP VIEW IF EXISTS SufficientChromeProcesses;

CREATE VIEW SufficientChromeProcesses AS
  SELECT
    CASE WHEN (
      SELECT COUNT(*) FROM ChromeProcessesInfo
    ) = 0 THEN
    FALSE ELSE (
      SELECT COUNT(*) >= 3 FROM (
        SELECT name FROM ChromeProcessesInfo GROUP BY name
    )) END AS haveEnoughProcesses;

-- A simple table that checks the time between VSync (this can be used to
-- determine if we're scrolling at 90 FPS or 60 FPS.
--
-- Note: In traces without the "Java" category there will be no VSync
--       TraceEvents
DROP TABLE IF EXISTS VSyncIntervals;

CREATE TABLE VSyncIntervals AS
  SELECT
    slice_id,
    ts,
    dur,
    track_id,
    LEAD(ts) OVER(PARTITION BY track_id ORDER BY ts) - ts AS timeToNextVsync
  FROM slice
  WHERE name = "VSync"
  ORDER BY track_id, ts;

-- Get all the GestureScrollBegin and GestureScrollEnd events. We take their
-- IDs to group them together into scrolls later and the timestamp and duration
-- to compute the duration of the scroll.
DROP VIEW IF EXISTS ScrollBeginsAndEnds;

CREATE VIEW ScrollBeginsAndEnds AS
  SELECT
    slice.name,
    slice.id,
    slice.ts,
    slice.dur,
    slice.track_id,
    EXTRACT_ARG(arg_set_id, 'chrome_latency_info.gesture_scroll_id')
        AS gestureScrollId,
    EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") AS traceId
  FROM
    slice
  WHERE
    slice.name IN (
      'InputLatency::GestureScrollBegin',
      'InputLatency::GestureScrollEnd'
    )
  ORDER BY ts;

-- Now we take the Begin and the End events and join the information into a
-- single row per scroll. We also compute the average Vysnc interval of the
-- scroll (hopefully this would be either 60 FPS for the whole scroll or 90 FPS
-- but that isn't always the case). If the trace doesn't contain the VSync
-- TraceEvent we just fall back on assuming its 60 FPS.
DROP VIEW IF EXISTS JoinedScrollBeginsAndEnds;

CREATE VIEW JoinedScrollBeginsAndEnds AS
  SELECT
    begin.id AS beginId,
    begin.ts AS scrollBegin,
    begin.dur AS scrollBeginDur,
    begin.track_id AS scrollBeginTrackId,
    begin.traceId AS scrollBeginTraceId,
    COALESCE(begin.gestureScrollId, begin.traceId) as gestureScrollId,
    end.ts AS scrollEndTs,
    end.ts + end.dur AS maybeScrollEnd,
    end.traceId AS scrollEndTraceId,
    COALESCE((
      SELECT
        CAST(AVG(timeToNextVsync) AS FLOAT)
      FROM VsyncIntervals in_query
      WHERE
        timeToNextVsync IS NOT NULL AND
        in_query.ts > begin.ts AND
        in_query.ts < end.ts
    ), 1.6e+7) AS scrollAvgVsyncInterval
  FROM ScrollBeginsAndEnds begin JOIN ScrollBeginsAndEnds end ON
    begin.traceId < end.traceId AND
    begin.name = 'InputLatency::GestureScrollBegin' AND
    end.name = 'InputLatency::GestureScrollEnd' AND (
      (
        begin.gestureScrollId IS NULL AND
        end.traceId = (
          SELECT MIN(traceId)
          FROM ScrollBeginsAndEnds in_query
          WHERE
            name = 'InputLatency::GestureScrollEnd' AND
          in_query.traceId > begin.traceId
        )
      ) OR
      end.gestureScrollId = begin.gestureScrollId
    )
  ORDER BY begin.ts;

-- Get the GestureScrollUpdate events by name ordered by timestamp, compute
-- the number of frames (relative to 60 fps) that each event took. 1.6e+7 is
-- 16 ms in nanoseconds. We also each GestureScrollUpdate event to the
-- information about it's begin and end for easy computation later.
--
-- We remove updates with |dur| == -1 because this means we have no end event
-- and can't reasonably determine what it should be. We have separate tracking
-- to ensure this only happens at the end of the trace.
--
-- Since we're using a window function to ensure consistent results we create a
-- TABLE rather than a VIEW.
DROP TABLE IF EXISTS GestureScrollUpdates;

CREATE TABLE GestureScrollUpdates AS
  SELECT
    ROW_NUMBER() OVER (
      ORDER BY scrollGestureScrollId ASC, ts ASC) AS rowNumber,
    beginId,
    scrollBegin,
    scrollBeginDur,
    scrollBeginTrackId,
    scrollBeginTraceId,
    COALESCE(scrollGestureScrollId, scrollBeginTraceId)
        AS scrollGestureScrollId,
    CASE WHEN
      maybeScrollEnd > ts + dur THEN
        maybeScrollEnd ELSE
        ts + dur
      END AS scrollEnd,
    id,
    ts,
    dur,
    track_id,
    traceId,
    dur/scrollAvgVsyncInterval AS scrollFramesExact
  FROM JoinedScrollBeginsAndEnds beginAndEnd JOIN (
    SELECT
      EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") AS traceId,
      EXTRACT_ARG(arg_set_id, 'chrome_latency_info.gesture_scroll_id')
        AS scrollGestureScrollId,
      *
    FROM
      slice slice JOIN track ON slice.track_id = track.id
    WHERE
      slice.name = 'InputLatency::GestureScrollUpdate' AND
      slice.dur != -1 AND
      NOT EXTRACT_ARG(arg_set_id, "chrome_latency_info.is_coalesced")
  ) scrollUpdate ON
  scrollUpdate.ts <= beginAndEnd.scrollEndTs AND
  scrollUpdate.ts >= beginAndEnd.scrollBegin AND
  scrollUpdate.traceId > beginAndEnd.scrollBeginTraceId AND
  scrollUpdate.traceId < beginAndEnd.scrollEndTraceId AND (
    scrollUpdate.scrollGestureScrollId IS NULL OR
    scrollUpdate.scrollGestureScrollId = beginAndEnd.gestureScrollId
  );

-- This takes the GestureScrollUpdate and joins it to the previous row (NULL
-- if there isn't one) and the next row (NULL if there isn't one). And then
-- computes if the duration of the event (relative to 60 fps) increased by more
-- then 0.5 (which is 1/2 of 16 ms and hopefully eventually replaced with vsync
-- interval).
--
-- We only compare a ScrollUpdate within its scroll
-- (currBeginId == prev/next BeginId). This controls somewhat for variability
-- of scrolls.
--
-- Since we're using a window function to ensure consistent results we create a
-- TABLE rather than a VIEW.
DROP TABLE IF EXISTS ScrollJanksMaybeNull;

CREATE TABLE ScrollJanksMaybeNull AS
  SELECT
    ROW_NUMBER() OVER (
      ORDER BY currGestureScrollId ASC, currprev.ts ASC) AS rowNumber,
    currScrollDur,
    currprev.id,
    currprev.ts,
    currprev.dur,
    currprev.track_id,
    currprev.traceId,
    currGestureScrollId,
    CASE WHEN
      currGestureScrollId != prevGestureScrollId OR
      prevTs IS NULL OR
      prevTs < currScrollBeginTs OR
      prevTs > currScrollEndTs
    THEN
      0 ELSE
      currScrollFramesExact > prevScrollFramesExact + 0.5
    END AS prevJank,
    CASE WHEN
      currGestureScrollId != next.scrollGestureScrollId OR
      next.ts IS NULL OR
      next.ts < currScrollBeginTs OR
      next.ts > currScrollEndTs
    THEN
      0 ELSE
      currScrollFramesExact > next.ScrollFramesExact + 0.5
    END AS nextJank,
    prevScrollFramesExact,
    currScrollFramesExact,
    next.ScrollFramesExact as nextScrollFramesExact
  FROM (
    SELECT
      curr.id,
      curr.ts,
      curr.dur,
      curr.track_id,
      curr.traceId AS traceId,
      curr.scrollEnd AS currScrollEndTs,
      curr.ScrollBegin AS currScrollBeginTs,
      curr.scrollEnd - curr.ScrollBegin as currScrollDur,
      curr.rowNumber AS currRowNumber,
      curr.scrollFramesExact AS currScrollFramesExact,
      curr.scrollGestureScrollId AS currGestureScrollId,
      prev.ts AS prevTs,
      prev.beginId AS prevBeginId,
      prev.scrollGestureScrollId as prevGestureScrollId,
      prev.scrollFramesExact AS prevScrollFramesExact
    FROM
      GestureScrollUpdates curr LEFT JOIN
      GestureScrollUpdates prev ON prev.rowNumber + 1 = curr.rowNumber
  ) currprev LEFT JOIN
  GestureScrollUpdates next ON currprev.currRowNumber + 1 = next.rowNumber
  ORDER BY currprev.currGestureScrollId ASC, currprev.ts ASC;

-- This just lists outs the rowNumber (which is ordered by timestamp), its jank
-- status and information about the update and scroll overall. Basically
-- getting it into a next queriable format.
DROP VIEW IF EXISTS ScrollJanks;

CREATE VIEW ScrollJanks AS
  SELECT
    rowNumber,
    id,
    ts,
    dur,
    track_id,
    traceId,
    currScrollDur,
    currGestureScrollId,
    (nextJank IS NOT NULL AND nextJank) OR
    (prevJank IS NOT NULL AND prevJank)
    AS jank
  FROM ScrollJanksMaybeNull
  ORDER BY currGestureScrollId ASC, ts ASC;

--------------------------------------------------------------------------------
-- BEGIN of blocking TouchMove computation.
--------------------------------------------------------------------------------

-- Below we want to collect TouchMoves and figure out if they blocked any
-- GestureScrollUpdates. This table gets the TouchMove slice and joins it with
-- the data from the first flow event for that TouchMove.
DROP TABLE IF EXISTS TouchMoveAndBeginFlow;

CREATE TABLE TouchMoveAndBeginFlow AS
  SELECT
    flow.beginSliceId, flow.beginTs, flow.beginTrackId, move.*
  FROM (
    SELECT
      EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") as traceId,
      *
    FROM slice move WHERE name = "InputLatency::TouchMove"
  ) move JOIN (
    SELECT
      min(slice_id) as beginSliceId,
      track_id as beginTrackId,
      ts as beginTs,
      EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") as traceId
    FROM slice
    WHERE
        name = "LatencyInfo.Flow" AND
        EXTRACT_ARG(arg_set_id, "chrome_latency_info.step") IS NULL
    GROUP BY traceId
  ) flow ON flow.traceId = move.traceId;

-- Now we take the TouchMove and beginning flow event and figured out if there
-- is an end flow event on the same browser track_id. This will allow us to see
-- if it was blocking because if they share the same parent stack then they
-- weren't blocking.
DROP TABLE IF EXISTS TouchMoveAndBeginEndFlow;

CREATE TABLE TouchMoveAndBeginEndFlow AS
  SELECT
    flow.endSliceId, flow.endTs, flow.endTrackId, move.*
  FROM TouchMoveAndBeginFlow move LEFT JOIN (
    SELECT
      max(slice_id) as endSliceId,
      ts AS endTs,
      track_id AS endTrackId,
      EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") as traceId
    FROM slice
    WHERE
        name = "LatencyInfo.Flow" AND
        EXTRACT_ARG(arg_set_id, "chrome_latency_info.step") IS NULL
    GROUP BY traceId
  ) flow ON
      flow.traceId = move.traceId AND
      move.beginTrackId = flow.endTrackId AND
      flow.endSliceId != move.beginSliceId
  WHERE flow.endSliceId IS NOT NULL;

-- Now that we have the being and the end we need to find the parent stack of
-- both. If the end didn't happen on the browser (end is NULL), then we can
-- ignore it because it couldn't have generated a GestureScrollUpdate.
DROP TABLE IF EXISTS TouchMoveWithParentBeginAndEndSlices;

CREATE TABLE TouchMoveWithParentBeginAndEndSlices AS
SELECT
  begin.slice_id AS beginSlice,
  end.slice_id AS endSlice,
  end.ts AS endSliceTs,
  end.dur AS endSliceDur,
  end.track_id AS endSliceTrackId,
  move.*
FROM TouchMoveAndBeginEndFlow move JOIN (
    SELECT slice_id, ts, dur, track_id, depth
    FROM slice in_query
  ) begin ON
      begin.depth = 0 AND
      begin.track_id = move.beginTrackId AND
      begin.ts < move.beginTs AND
      begin.ts + begin.dur > move.beginTs
  LEFT JOIN
  (
    SELECT slice_id, ts, dur, track_id, depth
    FROM slice in_query
  ) end ON
      end.depth = 0 AND
      end.track_id = move.endTrackId AND
      end.ts < move.endTs AND
      end.ts + end.dur > move.endTs;

-- Now take the parent stack for the end and find if a GestureScrollUpdate was
-- launched that share the same parent as the end flow event for the TouchMove.
-- This is the GestureScrollUpdate that the TouchMove blocked (or didn't block)
-- depending on if the begin flow event is in the same stack.
DROP TABLE IF EXISTS BlockingTouchMoveAndGestures;

CREATE TABLE BlockingTouchMoveAndGestures AS
  SELECT
      move.beginSlice != move.endSlice AS blockingTouchMove,
      scroll.gestureScrollFlowSliceId,
      scroll.gestureScrollFlowTraceId,
      scroll.gestureScrollSliceId,
      move.*
    FROM TouchMoveWithParentBeginAndEndSlices move LEFT JOIN (
      SELECT in_flow.*, in_scroll.gestureScrollSliceId FROM (
        SELECT
          min(slice_id) AS gestureScrollFlowSliceId,
          ts AS gestureScrollFlowTs,
          track_id AS gestureScrollFlowTrackId,
          EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id")
              AS gestureScrollFlowTraceId,
          (
            SELECT
              id
            FROM slice in_query
            WHERE
              in_query.depth = 0 AND
              in_query.track_id = out_query.track_id AND
              in_query.ts < out_query.ts AND
              in_query.ts + in_query.dur > out_query.ts
          ) AS gestureScrollFlowSlice
        FROM slice out_query
        WHERE
          name = "LatencyInfo.Flow" AND
          EXTRACT_ARG(arg_set_id, "chrome_latency_info.step") IS NULL
        GROUP BY gestureScrollFlowTraceId
      ) in_flow JOIN (
        SELECT
          slice_id AS gestureScrollSliceId,
          EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id")
              AS gestureScrollTraceId
        FROM slice in_scroll
        WHERE
          name = "InputLatency::GestureScrollUpdate" AND
	  dur != -1 AND
	  NOT EXTRACT_ARG(arg_set_id, "chrome_latency_info.is_coalesced")
      ) in_scroll ON
          in_scroll.gestureScrollTraceId = in_flow.gestureScrollFlowTraceId
    ) scroll ON
      scroll.gestureScrollFlowTrackId = move.endSliceTrackId AND
      scroll.gestureScrollFlowSlice = move.endSlice AND
      scroll.gestureScrollFlowTs > move.endSliceTs AND
      scroll.gestureScrollFlowTs < move.endSliceTs + move.endSliceDur AND
      scroll.gestureScrollFlowSliceId > move.endSlice
    WHERE scroll.gestureScrollSliceId IS NOT NULL;

-- Now filter out any TouchMoves that weren't during a complete scroll. Most of
-- the other ones will be null anyway since they won't have
-- GestureScrollUpdates.
DROP VIEW IF EXISTS BlockingTouchMoveAndGesturesInScroll;

CREATE VIEW BlockingTouchMoveAndGesturesInScroll AS
   SELECT
      slice_id, ts, dur, track_id, blockingTouchMove, gestureScrollSliceId
  FROM JoinedScrollBeginsAndEnds beginAndEnd JOIN (
    SELECT
      *
    FROM BlockingTouchMoveAndGestures
  ) touch ON
    touch.ts <= beginAndEnd.scrollEndTs AND
    touch.ts > beginAndEnd.scrollBegin + beginAndEnd.scrollBeginDur AND
    touch.traceId > beginAndEnd.scrollBeginTraceId AND
    touch.traceId < beginAndEnd.scrollEndTraceId;

--------------------------------------------------------------------------------
-- END of blocking TouchMove computation.
--------------------------------------------------------------------------------

-- Join the blockingTouchMove to ScrollJanks table so we can see if potentially
-- the blockingTouchMove caused the GestureScrollUpdate to be janky.
DROP VIEW IF EXISTS ScrollJanksAndCauses;

CREATE VIEW ScrollJanksAndCauses AS
  SELECT
    COALESCE(move.blockingTouchMove, 0) AS blockingTouchMove,
    jank.*
  FROM
    ScrollJanks jank LEFT JOIN BlockingTouchMoveAndGesturesInScroll move ON
        move.gestureScrollSliceId = jank.id;

-- Compute the total amount of nanoseconds from Janky GestureScrollUpdates and
-- the total amount of nanoseconds we spent scrolling in the trace. Also need
-- to get the total time on all GestureScrollUpdates. We cast them to float to
-- ensure our devision results in a percentage and not just integer division
-- equal to 0.
--
-- We need to select MAX(currScrollDur) because the last few
-- GestureScrollUpdates might extend past the GestureScrollEnd event so we
-- select the number which is the last part of the scroll. See b/150867143 for
-- context.

DROP VIEW IF EXISTS JankyNanosPerScrollNanosMaybeNull;

CREATE VIEW JankyNanosPerScrollNanosMaybeNull AS
  SELECT
    SUM(jank) as numJankyUpdates,
    CAST(SUM(CASE WHEN jank = 1 THEN
      dur ELSE
      0 END) AS FLOAT) AS jankyNanos,
    CAST(SUM(CASE WHEN jank = 1 AND blockingTouchMove THEN
      dur ELSE
      0 END) AS FLOAT) AS jankyTouchMoveNanos,
    CAST(SUM(dur) AS FLOAT) AS totalProcessingNanos,
    (
      SELECT sum(scrollDur)
      FROM (
        SELECT
          MAX(currScrollDur) AS scrollDur
        FROM ScrollJanks
        GROUP BY currGestureScrollId
      )
    ) AS scrollNanos
  FROM ScrollJanksAndCauses;

DROP VIEW IF EXISTS janky_time_per_scroll_processing_time;

CREATE VIEW janky_time_per_scroll_processing_time AS
  SELECT
    (SELECT
      CASE WHEN
        totalProcessingNanos IS NULL OR jankyNanos IS NULL OR
        totalProcessingNanos = 0 THEN
          0.0 ELSE
          (
          CAST(jankyNanos AS FLOAT) /
          CAST(totalProcessingNanos AS FLOAT)
          ) * 100.0 END
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
    FROM JankyNanosPerScrollNanosMaybeNull) AS numJankyUpdates,
    (SELECT
      COALESCE(jankyTouchMoveNanos, 0) / 1000000
    FROM JankyNanosPerScrollNanosMaybeNull) AS jankyTouchMoveMillis,
    (SELECT
      CASE WHEN
        totalProcessingNanos IS NULL OR jankyTouchMoveNanos IS NULL OR
        totalProcessingNanos = 0 THEN
          0.0 ELSE
          (
          CAST(jankyTouchMoveNanos AS FLOAT) /
          CAST(totalProcessingNanos AS FLOAT)
          ) * 100.0 END
    FROM JankyNanosPerScrollNanosMaybeNull) AS jankyTouchMovePercentage,
    (SELECT
      CASE WHEN
        jankyNanos IS NULL OR jankyTouchMoveNanos IS NULL OR
        jankyNanos = 0 THEN
          0.0 ELSE
          (
          CAST(jankyTouchMoveNanos AS FLOAT) /
          CAST(jankyNanos AS FLOAT)
          ) * 100.0 END
    FROM JankyNanosPerScrollNanosMaybeNull) AS jankyTouchMovePerJankyPercentage;

-- Specify how to fill the metrics proto properly.

DROP VIEW IF EXISTS janky_time_per_scroll_processing_time_output;

CREATE VIEW janky_time_per_scroll_processing_time_output AS
  SELECT JankyTimePerScrollProcessingTime(
    'janky_time_per_scroll_processing_time_percentage', jankyPercentage,
    'gesture_scroll_milliseconds', CAST(scrollMillis AS INT),
    'janky_gesture_scroll_milliseconds', CAST(jankyMillis AS INT),
    'gesture_scroll_processing_milliseconds', CAST(processingMillis AS INT),
    'num_janky_gesture_scroll_updates', CAST(numJankyUpdates AS INT),
    'janky_touch_move_gesture_scroll_milliseconds',
    CAST(jankyTouchMoveMillis AS INT),
    'janky_touch_move_time_per_scroll_processing_time_percentage',
    jankyTouchMovePercentage,
    'janky_touch_move_time_per_janky_processing_time_percentage',
    jankyTouchMovePerJankyPercentage)
  FROM janky_time_per_scroll_processing_time;
