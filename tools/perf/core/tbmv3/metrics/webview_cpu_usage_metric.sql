SELECT RUN_METRIC('webview/webview_cpu_usage.sql');

CREATE VIEW webview_cpu_usage_metric_output AS
SELECT WebViewCPUUsageMetric(
    'cpu_time_nanos_below_1ghz',
    (SELECT IFNULL(CAST(SUM(runtime_ns) AS INT), 0)
    FROM
      webview_raw_metrics_per_core
    WHERE avg_freq_khz < 1000000),
    'cpu_time_nanos_above_1ghz',
    (SELECT IFNULL(CAST(SUM(runtime_ns) AS INT), 0)
    FROM
      webview_raw_metrics_per_core
    WHERE avg_freq_khz >= 1000000)
);
