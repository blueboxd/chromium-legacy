-- Copyright 2019 Google LLC.
-- SPDX-License-Identifier: Apache-2.0

CREATE VIEW dummy_metric_output AS
SELECT DummyMetric('foo', 42)
