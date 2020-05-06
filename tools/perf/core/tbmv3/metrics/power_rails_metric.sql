-- A metric that collects on-device power rails measurement for the
-- duration of the story run. Power drain breakdown is device-specific
-- since different devices have different sensors.
-- See go/power-mobile-benchmark for the list of supported devices.
-- Output values are in Joules (Watt-seconds).

CREATE VIEW RunStory AS
SELECT
    MIN(ts) AS begin,
    MAX(ts + dur) AS end
FROM slice
WHERE name LIKE '%.RunStory';

CREATE VIEW drains AS
SELECT
    counter_track.name as name,
    (MAX(counter.value) - MIN(counter.value)) / 1e6 as drain_j
FROM counter
JOIN counter_track ON (counter.track_id = counter_track.id)
WHERE
    counter_track.type = 'counter_track'
    AND counter.ts >= (SELECT begin FROM RunStory)
    AND counter.ts <= (SELECT end FROM RunStory)
GROUP BY counter_track.name;

CREATE VIEW power_rails_metric_output AS
SELECT PowerRailsMetric(
  'total_j',
      (SELECT sum(drain_j) FROM drains WHERE name LIKE 'power.%'),
  'cpu_big_core_cluster_j',
      (SELECT drain_j FROM drains WHERE name = 'power.VPH_PWR_S5C_S6C_uws'),
  'cpu_little_core_cluster_j',
      (SELECT drain_j FROM drains WHERE name = 'power.VPH_PWR_S4C_uws'),
  'soc_j',
      (SELECT drain_j FROM drains WHERE name = 'power.VPH_PWR_S2C_S3C_uws'),
  'display_j',
      (SELECT drain_j FROM drains WHERE name = 'power.VPH_PWR_OLED_uws'),
  'duration_ms',
      (SELECT (end - begin) / 1e6 from RunStory)
);
