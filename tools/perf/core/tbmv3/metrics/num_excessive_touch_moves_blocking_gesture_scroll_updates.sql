
-- This metric outputs how many times a blocking TouchMove caused a back log of
-- GestureScrollUpdates. When a TouchMove is blocking GestureScrollUpdates can't
-- be responded to until the TouchMove is handled, this can include having to
-- wait for a busy renderer. We want to determine how frequently this issue
-- happens.
--
-- See b/153323740 for original investigation.
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
    );

-- Now we take the Begin and the End events and join the information into a
-- single row per scroll.

DROP VIEW IF EXISTS JoinedScrollBeginsAndEnds;

CREATE VIEW JoinedScrollBeginsAndEnds AS
  SELECT
    begin.id AS beginId,
    begin.ts AS scrollBegin,
    begin.dur AS scrollBeginDur,
    begin.track_id AS scrollBeginTrackId,
    begin.traceId AS scrollBeginTraceId,
    begin.gestureScrollId as gestureScrollId,
    end.ts AS scrollEndTs,
    end.ts + end.dur AS maybeScrollEnd,
    end.traceId AS scrollEndTraceId
  FROM ScrollBeginsAndEnds begin JOIN ScrollBeginsAndEnds end ON
    begin.traceId < end.traceId AND
    begin.name = 'InputLatency::GestureScrollBegin' AND
    end.name = 'InputLatency::GestureScrollEnd' AND
    end.ts = (
      SELECT MIN(ts)
      FROM ScrollBeginsAndEnds in_query
      WHERE
        name = 'InputLatency::GestureScrollEnd' AND
      in_query.ts > begin.ts
    ) AND
    end.gestureScrollId = begin.gestureScrollId;

-- This table grabs each TouchMove and counts how many TouchMoves and
-- GestureScrollUpdates queued after the TouchMove and overlaps with it. Its
-- important to track the issue found in b/153323740 to only include TouchMoves
-- which are after a GestureScrollBegin. This is because a blocking TouchMove
-- before we know we're scrolling isn't always a bug if the website has a touch
-- handler registered.
DROP VIEW IF EXISTS OverlappingTouchAndGestures;

CREATE VIEW OverlappingTouchAndGestures AS
   SELECT
      slice_id, ts, dur, track_id,
      (
        SELECT COUNT(*)
        FROM slice overlapTouch
        WHERE
          overlapTouch.name = "InputLatency::TouchMove" AND
          touch.ts <= overlapTouch.ts AND
          touch.ts + touch.dur >= overlapTouch.ts AND
          touch.id != overlapTouch.id
      ) AS numOverlappingTouchMoves,
      (
        SELECT COUNT(*)
        FROM slice overlapGesture
        WHERE
          overlapGesture.name = "InputLatency::GestureScrollUpdate" AND
          touch.ts <= overlapGesture.ts AND
          touch.ts + touch.dur >= overlapGesture.ts
      ) AS numOverlappingGestureScrollUpdates
  FROM  JoinedScrollBeginsAndEnds beginAndEnd JOIN (
    SELECT
      *,
      EXTRACT_ARG(arg_set_id, "chrome_latency_info.trace_id") as traceId
    FROM slice
    WHERE
      slice.name = "InputLatency::TouchMove"
  ) touch ON
    touch.ts <= beginAndEnd.scrollEndTs AND
    touch.ts > beginAndEnd.scrollBegin + beginAndEnd.scrollBeginDur AND
    touch.traceId > beginAndEnd.scrollBeginTraceId AND
    touch.traceId < beginAndEnd.scrollEndTraceId;

-- To prevent sqlite weirdness we create a table to hold the row number, and we
-- also define excessive by the constants chossen. These values were chosen by
-- inspecting traces for what looks out of place.
DROP TABLE IF EXISTS ExcessiveTouchAndGestureOverlap;

CREATE TABLE ExcessiveTouchAndGestureOverlap AS
  SELECT
    ROW_NUMBER() OVER (ORDER BY ts ASC) AS rowNumber,
    slice_id, ts, dur, track_id,
    numOverlappingTouchMoves,
    numOverlappingGestureScrollUpdates,
    numOverlappingGestureScrollUpdates > 10 AND numOverlappingTouchMoves > 10
      AS hasExcessiveOverlap
  FROM OverlappingTouchAndGestures;

-- We want to count how often it occurred not its severity so we just check to
-- see if this is the first one, (i.e the previous is null OR the previous
-- doesn't have excessive overlap), so we can count frequency.
DROP VIEW IF EXISTS OnlyFirstExcessiveTouchAndGestureOverlap;

CREATE VIEW OnlyFirstExcessiveTouchAndGestureOverlap AS
  SELECT
    curr.slice_id, curr.ts, curr.dur, curr.track_id,
    curr.numOverlappingTouchMoves,
    curr.numOverlappingGestureScrollUpdates,
    CASE WHEN
      prev.hasExcessiveOverlap IS NULL OR NOT prev.hasExcessiveOverlap THEN
        curr.hasExcessiveOverlap ELSE
        0
      END AS hasFirstExcessiveOverlap
  FROM
    ExcessiveTouchAndGestureOverlap curr LEFT JOIN
    ExcessiveTouchAndGestureOverlap prev ON
      curr.rowNumber - 1 = prev.rowNumber;

DROP VIEW IF EXISTS TraceHasScroll;

CREATE VIEW TraceHasScroll AS
  SELECT SUM(dur) AS scrollDuration
  FROM slice
  WHERE name = "InputLatency::GestureScrollUpdate";

-- Name the metric and output the result (recall that booleans are 1 if true so
-- SUM(boolean) is just the number of occurrences).
--
-- We select the sum inside a sub-query because if there is no rows SUM()
-- returns null which results in an error in the chrometto pipeline.

CREATE VIEW num_excessive_touch_moves_blocking_gesture_scroll_updates_output AS
  SELECT NumExcessiveTouchMovesBlockingGestureScrollUpdates(
  "num_touch_moves_blocking_gesture_scrolls", sumExcessiveOverlaps)
FROM (
  SELECT
    COALESCE(SUM(hasFirstExcessiveOverlap), 0) AS sumExcessiveOverlaps
  FROM OnlyFirstExcessiveTouchAndGestureOverlap
)
WHERE
  (
    SELECT
      CASE WHEN scrollDuration IS NOT NULL THEN
        scrollDuration ELSE
        0 END
    FROM TraceHasScroll
  ) > 0;
