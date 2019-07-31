// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CalculatedMetric {
  constructor(insertAtOriginalMetricName, name, value) {
    this.insertAtOriginalMetricName = insertAtOriginalMetricName;
    this.name = name;
    this.value = value;
  }
}

class StatsReport {
  constructor() {
    // Represents an RTCStatsReport. It is a Map RTCStats.id -> RTCStats.
    // https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport
    this.statsById = new Map();
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
        const calculatedMetric =
            this.getCalculatedMetricByOriginalName(stats.id, metricName);
        if (calculatedMetric) {
          internalReport.stats.values.push(calculatedMetric.name);
          // Treat calculated metrics that are undefined as 0 to ensure graphs
          // can be created anyway.
          internalReport.stats.values.push(
              calculatedMetric.value ? calculatedMetric.value : 0);
        }
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
      str2 += JSON.stringify(stats);
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

  setCalculatedMetric(id, insertAtOriginalMetricName, name, value) {
    let calculatedStats = this.calculatedStatsById.get(id);
    if (!calculatedStats) {
      calculatedStats = {};
      this.calculatedStatsById.set(id, calculatedStats);
    }
    calculatedStats[insertAtOriginalMetricName] =
        new CalculatedMetric(insertAtOriginalMetricName, name, value);
  }

  getCalculatedMetricByOriginalName(id, originalMetricName) {
    const calculatedStats = this.calculatedStatsById.get(id);
    return calculatedStats ? calculatedStats[originalMetricName] : undefined;
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
                    this.currentReport.setCalculatedMetric(
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
