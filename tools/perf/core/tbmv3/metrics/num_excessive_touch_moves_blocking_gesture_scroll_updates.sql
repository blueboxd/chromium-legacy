
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
    )
  ORDER BY ts;

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
    COALESCE(begin.gestureScrollId, begin.traceId) as gestureScrollId,
    end.ts AS scrollEndTs,
    end.ts + end.dur AS maybeScrollEnd,
    end.traceId AS scrollEndTraceId
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

-- Because we're using a windowing function make sure this is a table.
DROP TABLE IF EXISTS TouchMovesBlocking;

CREATE TABLE TouchMovesBlocking AS
  SELECT
    ROW_NUMBER() OVER (ORDER BY ts ASC) AS rowNumber,
    slice_id, ts, dur, track_id,
    blockingTouchMove,
    gestureScrollSliceId
  FROM BlockingTouchMoveAndGesturesInScroll out_query
  ORDER BY ts ASC;

-- We want to count how often it occurred not its severity so we just check to
-- see if this is the first one, (i.e the previous is null OR the previous
-- doesn't have excessive overlap), so we can count frequency.
DROP VIEW IF EXISTS FirstTouchMovesBlocking;

CREATE VIEW FirstTouchMovesBlocking AS
  SELECT
    curr.slice_id, curr.ts, curr.dur, curr.track_id,
    curr.blockingTouchMove,
    CASE WHEN
      prev.blockingTouchMove IS NULL OR NOT prev.blockingTouchMove THEN
        curr.blockingTouchMove ELSE
        0
      END AS hasFirstBlockingTouchMoves
  FROM
    TouchMovesBlocking curr LEFT JOIN
    TouchMovesBlocking prev ON
      curr.rowNumber - 1 = prev.rowNumber;

DROP VIEW IF EXISTS TraceHasScroll;

CREATE VIEW TraceHasScroll AS
  SELECT SUM(dur) AS scrollDuration
  FROM slice
  WHERE name = "InputLatency::GestureScrollUpdate";

DROP VIEW IF EXISTS FinalResults;
-- Name the metric and output the result (recall that booleans are 1 if true so
-- SUM(boolean) is just the number of occurrences).
CREATE VIEW FinalResults AS
  SELECT
    sumTouchMovesBecameBlocking,
    sumTouchMovesBlocking,
    sumTouchMoves
  FROM (
    SELECT
      SUM(hasFirstBlockingTouchMoves) AS sumTouchMovesBecameBlocking,
      SUM(COALESCE(blockingTouchMove, 0)) AS sumTouchMovesBlocking,
      count(*) AS sumTouchMoves
    FROM FirstTouchMovesBlocking
  )
  WHERE (
    SELECT
      CASE WHEN scrollDuration IS NOT NULL THEN
        scrollDuration ELSE
        0 END
    FROM TraceHasScroll
  ) > 0;

CREATE VIEW num_excessive_touch_moves_blocking_gesture_scroll_updates_output AS
  SELECT NumExcessiveTouchMovesBlockingGestureScrollUpdates(
      "num_touch_moves_blocking_gesture_scrolls",
      (SELECT sumTouchMovesBecameBlocking FROM FinalResults),
      "num_total_touch_moves_blocking_gesture_scrolls",
      (SELECT sumTouchMovesBlocking FROM FinalResults),
      "num_touch_moves_during_scroll",
      (SELECT sumTouchMoves FROM FinalResults)
  );
