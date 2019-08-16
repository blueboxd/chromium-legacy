// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Metric {
  constructor(name, value) {
    this.name = name;
    this.value = value;
  }

  toString() {
    return '{"' + this.name + '":"' + this.value + '"}';
  }
}

// Represents a companion dictionary to an RTCStats object of an RTCStatsReport.
// The CalculatedStats object contains additional metrics associated with the
// original RTCStats object. Typically, the RTCStats object contains
// accumulative counters, but in chrome://webrc-internals/ we also want graphs
// for the average rate over the last second, so we have CalculatedStats
// containing calculated Metrics.
class CalculatedStats {
  constructor(id) {
    this.id = id;
    // A map Original Name -> Array of Metrics, where Original Name refers to
    // the name of the metric in the original RTCStats object, and the Metrics
    // are calculated metrics. For example, if the original RTCStats report
    // contains framesReceived, and from that we've calculated
    // [framesReceived/s] and [framesReceived-framesDecoded], then there will be
    // a mapping from "framesReceived" to an array of two Metric objects,
    // "[framesReceived/s]" and "[framesReceived-framesDecoded]".
    this.calculatedMetricsByOriginalName = new Map();
  }

  addCalculatedMetric(originalName, metric) {
    let calculatedMetrics =
        this.calculatedMetricsByOriginalName.get(originalName);
    if (!calculatedMetrics) {
      calculatedMetrics = [];
      this.calculatedMetricsByOriginalName.set(originalName, calculatedMetrics);
    }
    calculatedMetrics.push(metric);
  }

  // Gets the calculated metrics associated with |originalName| in the order
  // that they were added, or an empty list if there are no associated metrics.
  getCalculatedMetrics(originalName) {
    let calculatedMetrics =
        this.calculatedMetricsByOriginalName.get(originalName);
    if (!calculatedMetrics) {
      return [];
    }
    return calculatedMetrics;
  }

  toString() {
    let str = '{id:"' + this.id + '"';
    for (let originalName of this.calculatedMetricsByOriginalName.keys()) {
      const calculatedMetrics =
          this.calculatedMetricsByOriginalName.get(originalName);
      str += ',' + originalName + ':[';
      for (let i = 0; i < calculatedMetrics.length; i++) {
        str += calculatedMetrics[i].toString();
        if (i + 1 < calculatedMetrics.length) {
          str += ',';
        }
        str += ']';
      }
    }
    str += '}';
    return str;
  }
}

class StatsReport {
  constructor() {
    // Represents an RTCStatsReport. It is a Map RTCStats.id -> RTCStats.
    // https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport
    this.statsById = new Map();
    // RTCStats.id -> CalculatedStats
    this.calculatedStatsById = new Map();
  }

  // |internalReports| is an array, each element represents an RTCStats object,
  // but the format is a little different from the spec. This is the format:
  // {
  //   id: "string",
  //   type: "string",
  //   stats: {
  //     timestamp: <milliseconds>,
  //     values: ["member1", value1, "member2", value2...]
  //   }
  // }
  static fromInternalsReportList(internalReports) {
    const result = new StatsReport();
    internalReports.forEach(internalReport => {
      if (!internalReport.stats || !internalReport.stats.values) {
        return;  // continue;
      }
      const stats = {
        id: internalReport.id,
        type: internalReport.type,
        timestamp: internalReport.stats.timestamp / 1000.0  // ms -> s
      };
      const values = internalReport.stats.values;
      for (let i = 0; i < values.length; i += 2) {
        // Metric "name: value".
        stats[values[i]] = values[i + 1];
      }
      result.statsById.set(stats.id, stats);
    });
    return result;
  }

  toInternalsReportList() {
    const result = [];
    for (let stats of this.statsById.values()) {
      const internalReport = {
        id: stats.id,
        type: stats.type,
        stats: {
          timestamp: stats.timestamp * 1000.0,  // s -> ms
          values: []
        }
      };
      Object.keys(stats).forEach(metricName => {
        if (metricName == 'id' || metricName == 'type' ||
            metricName == 'timestamp') {
          return;  // continue;
        }
        internalReport.stats.values.push(metricName);
        internalReport.stats.values.push(stats[metricName]);
        const calculatedMetrics =
            this.getCalculatedMetrics(stats.id, metricName);
        calculatedMetrics.forEach(calculatedMetric => {
          internalReport.stats.values.push(calculatedMetric.name);
          // Treat calculated metrics that are undefined as 0 to ensure graphs
          // can be created anyway.
          internalReport.stats.values.push(
              calculatedMetric.value ? calculatedMetric.value : 0);
        });
      });
      result.push(internalReport);
    }
    return result;
  }

  toString() {
    let str = '';
    for (let stats of this.statsById.values()) {
      if (str != '') {
        str += ',';
      }
      str += JSON.stringify(stats);
    }
    let str2 = '';
    for (let stats of this.calculatedStatsById.values()) {
      if (str2 != '') {
        str2 += ',';
      }
      str2 += stats.toString();
    }
    return '[original:' + str + '],calculated:[' + str2 + ']';
  }

  get(id) {
    return this.statsById.get(id);
  }

  getByType(type) {
    const result = [];
    for (let stats of this.statsById.values()) {
      if (stats.type == type) {
        result.push(stats);
      }
    }
    return result;
  }

  addCalculatedMetric(id, insertAtOriginalMetricName, name, value) {
    let calculatedStats = this.calculatedStatsById.get(id);
    if (!calculatedStats) {
      calculatedStats = new CalculatedStats(id);
      this.calculatedStatsById.set(id, calculatedStats);
    }
    calculatedStats.addCalculatedMetric(
        insertAtOriginalMetricName, new Metric(name, value));
  }

  getCalculatedMetrics(id, originalMetricName) {
    const calculatedStats = this.calculatedStatsById.get(id);
    return calculatedStats ?
        calculatedStats.getCalculatedMetrics(originalMetricName) :
        [];
  }
}

class StatsRatesCalculator {
  constructor() {
    this.previousReport = null;
    this.currentReport = null;
  }

  addStatsReport(report) {
    this.previousReport = this.currentReport;
    this.currentReport = report;
    this.updateCalculatedMetrics_();
  }

  // Updates all "calculated metrics", which are metrics derived from standard
  // values, such as converting total counters (e.g. bytesSent) to rates (e.g.
  // bytesSent/s).
  updateCalculatedMetrics_() {
    const calculatedMetrics = [
      {
        type: 'data-channel',
        condition: null,
        names: [
          ['messagesSent', 'timestamp'],
          ['messagesReceived', 'timestamp'],
          ['bytesSent', 'timestamp'],
          ['bytesReceived', 'timestamp'],
        ],
      },
      {
        type: 'track',
        condition: null,
        names: [
          ['framesSent', 'timestamp'],
          ['framesReceived', 'timestamp'],
          [
            'totalAudioEnergy',
            'totalSamplesDuration',
            '[Audio_Level_in_RMS]',
            (value) => {
              // Calculated according to:
              // https://w3c.github.io/webrtc-stats/#dom-rtcaudiohandlerstats-totalaudioenergy
              return Math.sqrt(value);
            },
          ],
          [
            'jitterBufferDelay',
            'jitterBufferEmittedCount',
            '[jitterBufferDelay/jitterBufferEmittedCount_in_ms]',
            (value) => {
              return value * 1000;  // s -> ms
            },
          ],
        ],
      },
      {
        type: 'outbound-rtp',
        condition: null,
        names: [
          ['bytesSent', 'timestamp'],
          ['packetsSent', 'timestamp'],
          [
            'totalPacketSendDelay',
            'packetsSent',
            '[totalPacketSendDelay/packetsSent_in_ms]',
            (value) => {
              return value * 1000;  // s -> ms
            },
          ],
          ['framesEncoded', 'timestamp'],
          [
            'totalEncodedBytesTarget', 'framesEncoded',
            '[targetEncodedBytes/s]',
            (value, currentStats, previousStats) => {
              if (!previousStats) {
                return 0;
              }
              const deltaTime =
                  currentStats.timestamp - previousStats.timestamp;
              const deltaFrames =
                  currentStats.framesEncoded - previousStats.framesEncoded;
              const encodedFrameRate = deltaFrames / deltaTime;
              return value * encodedFrameRate;
            }
          ],
          [
            'totalEncodeTime', 'framesEncoded',
            '[totalEncodeTime/framesEncoded_in_ms]',
            (value) => {
              return value * 1000;  // s -> ms
            }
          ],
          ['qpSum', 'framesEncoded'],
        ],
      },
      {
        type: 'inbound-rtp',
        condition: null,
        names: [
          ['bytesReceived', 'timestamp'],
          ['packetsReceived', 'timestamp'],
          ['framesDecoded', 'timestamp'],
          [
            'totalDecodeTime', 'framesDecoded',
            '[totalDecodeTime/framesDecoded_in_ms]',
            (value) => {
              return value * 1000;  // s -> ms
            }
          ],
          ['qpSum', 'framesDecoded'],
        ],
      },
      {
        type: 'transport',
        condition: null,
        names: [
          ['bytesSent', 'timestamp'], ['bytesReceived', 'timestamp'],
          // TODO(https://crbug.com/webrtc/10568): Add packetsSent and
          // packetsReceived once implemented.
        ],
      },
      {
        type: 'candidate-pair',
        condition: null,
        names: [
          ['bytesSent', 'timestamp'],
          ['bytesReceived', 'timestamp'],
          // TODO(https://crbug.com/webrtc/10569): Add packetsSent and
          // packetsReceived once implemented.
          ['requestsSent', 'timestamp'],
          ['requestsReceived', 'timestamp'],
          ['responsesSent', 'timestamp'],
          ['responsesReceived', 'timestamp'],
          ['consentRequestsSent', 'timestamp'],
          ['consentRequestsReceived', 'timestamp'],
          [
            'totalRoundTripTime',
            'responsesReceived',
            '[totalRoundTripTime/responsesReceived_in_ms]',
            (value) => {
              return value * 1000;  // s -> ms
            },
          ],
        ],
      },
    ];
    calculatedMetrics.forEach(calculatedMetric => {
      calculatedMetric.names.forEach(
          ([accumulativeMetric, samplesMetric, resultName, transformation]) => {
            this.currentReport.getByType(calculatedMetric.type)
                .forEach(stats => {
                  if (!calculatedMetric.condition ||
                      calculatedMetric.condition(stats)) {
                    if (!resultName) {
                      resultName = (samplesMetric == 'timestamp') ?
                          '[' + accumulativeMetric + '/s]' :
                          '[' + accumulativeMetric + '/' + samplesMetric + ']';
                    }
                    let result = this.calculateAccumulativeMetricOverSamples_(
                        stats.id, accumulativeMetric, samplesMetric);
                    if (result && transformation) {
                      const previousStats = this.previousReport.get(stats.id);
                      result = transformation(result, stats, previousStats);
                    }
                    this.currentReport.addCalculatedMetric(
                        stats.id, accumulativeMetric, resultName, result);
                  }
                });
          });
    });
  }

  // Calculates the rate "delta accumulative / delta samples" and returns it. If
  // a rate cannot be calculated, such as the metric is missing in the current
  // or previous report, undefined is returned.
  calculateAccumulativeMetricOverSamples_(
      id, accumulativeMetric, samplesMetric) {
    if (!this.previousReport || !this.currentReport) {
      return undefined;
    }
    const previousStats = this.previousReport.get(id);
    const currentStats = this.currentReport.get(id);
    if (!previousStats || !currentStats) {
      return undefined;
    }
    const deltaTime = currentStats.timestamp - previousStats.timestamp;
    if (deltaTime <= 0) {
      return undefined;
    }
    // Try to convert whatever the values are to numbers. This gets around the
    // fact that some types that are not supported by base::Value (e.g. uint32,
    // int64, uint64 and double) are passed as strings.
    const previousValue = Number(previousStats[accumulativeMetric]);
    const currentValue = Number(currentStats[accumulativeMetric]);
    if (typeof previousValue != 'number' || typeof currentValue != 'number') {
      return undefined;
    }
    const previousSamples = Number(previousStats[samplesMetric]);
    const currentSamples = Number(currentStats[samplesMetric]);
    if (typeof previousSamples != 'number' ||
        typeof currentSamples != 'number') {
      return undefined;
    }
    const deltaValue = currentValue - previousValue;
    const deltaSamples = currentSamples - previousSamples;
    return deltaValue / deltaSamples;
  }
}
