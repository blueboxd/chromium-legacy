// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/perf/perf_result_reporter.h"

namespace {

constexpr char kMetricPrefixAudioReceive[] = "WebRtcAudioReceive.";
constexpr char kMetricPrefixAudioSend[] = "WebRtcAudioSend.";
constexpr char kMetricPrefixVideoSend[] = "WebRtcVideoSend.";
constexpr char kMetricPrefixVideoRecieve[] = "WebRtcVideoReceive.";
constexpr char kMetricPrefixBwe[] = "WebRtcBwe.";
constexpr char kMetricPacketsLostFrames[] = "packets_lost";
constexpr char kMetricGoogJitterRecvMs[] = "goog_jitter_recv";
constexpr char kMetricGoogExpandRatePercent[] = "goog_expand_rate";
constexpr char kMetricGoogSpeechExpandRatePercent[] = "goog_speech_expand_rate";
constexpr char kMetricGoogSecondaryDecodeRatePercent[] =
    "goog_secondary_decode_rate";
constexpr char kMetricGoogRttMs[] = "goog_rtt";
constexpr char kMetricPacketsPerSecondPackets[] = "packets_per_second";
constexpr char kMetricGoogFpsSentFps[] = "goog_frame_rate_sent";
constexpr char kMetricGoogFpsInputFps[] = "goog_frame_rate_input";
constexpr char kMetricGoogFirsReceivedUnitless[] = "goog_firs_recv";
constexpr char kMetricGoogNacksReceivedUnitless[] = "goog_nacks_recv";
constexpr char kMetricGoogFrameWidthCount[] = "goog_frame_width";
constexpr char kMetricGoogFrameHeightCount[] = "goog_frame_height";
constexpr char kMetricGoogAvgEncodeMs[] = "goog_avg_encode";
constexpr char kMetricGoogEncodeCpuUsagePercent[] = "goog_encode_cpu_usage";
constexpr char kMetricGoogFpsRecvFps[] = "goog_frame_rate_recv";
constexpr char kMetricGoogFpsOutputFps[] = "goog_frame_rate_output";
constexpr char kMetricGoogActualDelayMs[] = "goog_actual_delay";
constexpr char kMetricGoogTargetDelayMs[] = "goog_target_delay";
constexpr char kMetricGoogDecodeTimeMs[] = "goog_decode_time";
constexpr char kMetricGoogMaxDecodeTimeMs[] = "goog_max_decode_time";
constexpr char kMetricGoogJitterBufferMs[] = "goog_jitter_buffer";
constexpr char kMetricGoogRenderDelayMs[] = "goog_render_delay";
constexpr char kMetricAvailableSendBandwidthBitsPerS[] = "available_send_bw";
constexpr char kMetricAvailableRecvBandwidthBitsPerS[] = "available_recv_bw";
constexpr char kMetricTargetEncodeBitrateBitsPerS[] = "target_encode_bitrate";
constexpr char kMetricActualEncodeBitrateBitsPerS[] = "actual_encode_bitrate";
constexpr char kMetricTransmitBitrateBitsPerS[] = "transmit_bitrate";

perf_test::PerfResultReporter SetUpAudioReceiveReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixAudioReceive, story);
  reporter.RegisterFyiMetric(kMetricPacketsLostFrames, "frames");
  reporter.RegisterFyiMetric(kMetricGoogJitterRecvMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogExpandRatePercent, "%");
  reporter.RegisterFyiMetric(kMetricGoogSpeechExpandRatePercent, "%");
  reporter.RegisterFyiMetric(kMetricGoogSecondaryDecodeRatePercent, "%");
  return reporter;
}

perf_test::PerfResultReporter SetUpAudioSendReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixAudioSend, story);
  reporter.RegisterFyiMetric(kMetricGoogJitterRecvMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogRttMs, "ms");
  reporter.RegisterFyiMetric(kMetricPacketsPerSecondPackets, "packets");
  return reporter;
}

perf_test::PerfResultReporter SetUpVideoSendReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixVideoSend, story);
  reporter.RegisterFyiMetric(kMetricGoogFpsSentFps, "fps");
  reporter.RegisterFyiMetric(kMetricGoogFpsInputFps, "fps");
  reporter.RegisterFyiMetric(kMetricGoogFirsReceivedUnitless, "unitless");
  reporter.RegisterFyiMetric(kMetricGoogNacksReceivedUnitless, "unitless");
  reporter.RegisterFyiMetric(kMetricGoogFrameWidthCount, "count");
  reporter.RegisterFyiMetric(kMetricGoogFrameHeightCount, "count");
  reporter.RegisterFyiMetric(kMetricGoogAvgEncodeMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogRttMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogEncodeCpuUsagePercent, "%");
  return reporter;
}

perf_test::PerfResultReporter SetUpVideoReceiveReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixVideoRecieve, story);
  reporter.RegisterFyiMetric(kMetricGoogFpsRecvFps, "fps");
  reporter.RegisterFyiMetric(kMetricGoogFpsOutputFps, "fps");
  reporter.RegisterFyiMetric(kMetricPacketsLostFrames, "frames");
  reporter.RegisterFyiMetric(kMetricGoogFrameWidthCount, "count");
  reporter.RegisterFyiMetric(kMetricGoogFrameHeightCount, "count");
  reporter.RegisterFyiMetric(kMetricGoogActualDelayMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogTargetDelayMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogDecodeTimeMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogMaxDecodeTimeMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogJitterBufferMs, "ms");
  reporter.RegisterFyiMetric(kMetricGoogRenderDelayMs, "ms");
  return reporter;
}

perf_test::PerfResultReporter SetUpBweReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixBwe, story);
  reporter.RegisterFyiMetric(kMetricAvailableSendBandwidthBitsPerS, "bits/s");
  reporter.RegisterFyiMetric(kMetricAvailableRecvBandwidthBitsPerS, "bits/s");
  reporter.RegisterFyiMetric(kMetricTargetEncodeBitrateBitsPerS, "bits/s");
  reporter.RegisterFyiMetric(kMetricActualEncodeBitrateBitsPerS, "bits/s");
  reporter.RegisterFyiMetric(kMetricTransmitBitrateBitsPerS, "bits/s");
  return reporter;
}

}  // namespace

static std::string Statistic(const std::string& statistic,
                             const std::string& bucket) {
  // A ssrc stats key will be on the form stats.<bucket>-<key>.values.
  // This will give a json "path" which will dig into the time series for the
  // specified statistic. Buckets can be for instance ssrc_1212344, bweforvideo,
  // and will each contain a bunch of statistics relevant to their nature.
  // Each peer connection has a number of such buckets.
  return base::StringPrintf("stats.%s-%s.values", bucket.c_str(),
                            statistic.c_str());
}

static void MaybePrintResultsForAudioReceive(
    const std::string& ssrc,
    const base::DictionaryValue& pc_dict,
    const std::string& story) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioOutputLevel", ssrc), &value)) {
    // Not an audio receive stream.
    return;
  }

  auto reporter = SetUpAudioReceiveReporter(story);
  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  reporter.AddResult(kMetricPacketsLostFrames, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogJitterRecvMs, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googExpandRate", ssrc), &value));
  reporter.AddResult(kMetricGoogExpandRatePercent, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googSpeechExpandRate", ssrc), &value));
  reporter.AddResult(kMetricGoogSpeechExpandRatePercent, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googSecondaryDecodedRate", ssrc), &value));
  reporter.AddResult(kMetricGoogSecondaryDecodeRatePercent, value);
}

static void MaybePrintResultsForAudioSend(const std::string& ssrc,
                                          const base::DictionaryValue& pc_dict,
                                          const std::string& story) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioInputLevel", ssrc), &value)) {
    // Not an audio send stream.
    return;
  }

  auto reporter = SetUpAudioSendReporter(story);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogJitterRecvMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  reporter.AddResult(kMetricGoogRttMs, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("packetsSentPerSecond", ssrc), &value));
  reporter.AddResult(kMetricPacketsPerSecondPackets, value);
}

static void MaybePrintResultsForVideoSend(const std::string& ssrc,
                                          const base::DictionaryValue& pc_dict,
                                          const std::string& story) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateSent", ssrc), &value)) {
    // Not a video send stream.
    return;
  }

  // Graph these by unit: the dashboard expects all stats in one graph to have
  // the same unit (e.g. ms, fps, etc). Most graphs, like video_fps, will also
  // be populated by the counterparts on the video receiving side.
  auto reporter = SetUpVideoSendReporter(story);
  reporter.AddResult(kMetricGoogFpsSentFps, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameRateInput", ssrc), &value));
  reporter.AddResult(kMetricGoogFpsInputFps, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFirsReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogFirsReceivedUnitless, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googNacksReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogNacksReceivedUnitless, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameWidthSent", ssrc), &value));
  reporter.AddResult(kMetricGoogFrameWidthCount, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightSent", ssrc), &value));
  reporter.AddResult(kMetricGoogFrameHeightCount, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googAvgEncodeMs", ssrc), &value));
  reporter.AddResult(kMetricGoogAvgEncodeMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  reporter.AddResult(kMetricGoogRttMs, value);

  EXPECT_TRUE(pc_dict.GetString(
      Statistic("googEncodeUsagePercent", ssrc), &value));
  reporter.AddResult(kMetricGoogEncodeCpuUsagePercent, value);
}

static void MaybePrintResultsForVideoReceive(
    const std::string& ssrc,
    const base::DictionaryValue& pc_dict,
    const std::string& story) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateReceived", ssrc), &value)) {
    // Not a video receive stream.
    return;
  }

  auto reporter = SetUpVideoReceiveReporter(story);
  reporter.AddResult(kMetricGoogFpsRecvFps, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameRateOutput", ssrc), &value));
  reporter.AddResult(kMetricGoogFpsOutputFps, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  reporter.AddResult(kMetricPacketsLostFrames, value);

  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameWidthReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogFrameWidthCount, value);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightReceived", ssrc), &value));
  reporter.AddResult(kMetricGoogFrameHeightCount, value);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googCurrentDelayMs", ssrc), &value));
  reporter.AddResult(kMetricGoogActualDelayMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googTargetDelayMs", ssrc), &value));
  reporter.AddResult(kMetricGoogTargetDelayMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googDecodeMs", ssrc), &value));
  reporter.AddResult(kMetricGoogDecodeTimeMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googMaxDecodeMs", ssrc), &value));
  reporter.AddResult(kMetricGoogMaxDecodeTimeMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterBufferMs", ssrc), &value));
  reporter.AddResult(kMetricGoogJitterBufferMs, value);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRenderDelayMs", ssrc), &value));
  reporter.AddResult(kMetricGoogRenderDelayMs, value);
}

static std::string ExtractSsrcIdentifier(const std::string& key) {
  // Example key: ssrc_1234-someStatName. Grab the part before the dash.
  size_t key_start_pos = 0;
  size_t key_end_pos = key.find("-");
  CHECK(key_end_pos != std::string::npos) << "Could not parse key " << key;
  return key.substr(key_start_pos, key_end_pos - key_start_pos);
}

// Returns the set of unique ssrc identifiers in the call (e.g. ssrc_1234,
// ssrc_12356, etc). |stats_dict| is the .stats dict from one peer connection.
static std::set<std::string> FindAllSsrcIdentifiers(
    const base::DictionaryValue& stats_dict) {
  std::set<std::string> result;
  base::DictionaryValue::Iterator stats_iterator(stats_dict);

  while (!stats_iterator.IsAtEnd()) {
    if (stats_iterator.key().find("ssrc_") != std::string::npos)
      result.insert(ExtractSsrcIdentifier(stats_iterator.key()));
    stats_iterator.Advance();
  }
  return result;
}

namespace test {

void PrintBweForVideoMetrics(const base::DictionaryValue& pc_dict,
                             const std::string& modifier,
                             const std::string& video_codec) {
  std::string story = video_codec.empty() ? "baseline_story" : video_codec;
  story += modifier;
  const std::string kBweStatsKey = "bweforvideo";
  std::string value;
  auto reporter = SetUpBweReporter(story);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableSendBandwidth", kBweStatsKey), &value));
  reporter.AddResult(kMetricAvailableSendBandwidthBitsPerS, value);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableReceiveBandwidth", kBweStatsKey), &value));
  reporter.AddResult(kMetricAvailableRecvBandwidthBitsPerS, value);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTargetEncBitrate", kBweStatsKey), &value));
  reporter.AddResult(kMetricTargetEncodeBitrateBitsPerS, value);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googActualEncBitrate", kBweStatsKey), &value));
  reporter.AddResult(kMetricActualEncodeBitrateBitsPerS, value);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTransmitBitrate", kBweStatsKey), &value));
  reporter.AddResult(kMetricTransmitBitrateBitsPerS, value);
}

void PrintMetricsForAllStreams(const base::DictionaryValue& pc_dict,
                               const std::string& modifier,
                               const std::string& video_codec) {
  PrintMetricsForSendStreams(pc_dict, modifier, video_codec);
  PrintMetricsForRecvStreams(pc_dict, modifier, video_codec);
}

void PrintMetricsForSendStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string story = video_codec.empty() ? "baseline_story" : video_codec;
  story += modifier;
  const base::DictionaryValue* stats_dict;
  ASSERT_TRUE(pc_dict.GetDictionary("stats", &stats_dict));
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioSend(ssrc, pc_dict, story);
    MaybePrintResultsForVideoSend(ssrc, pc_dict, story);
  }
}

void PrintMetricsForRecvStreams(const base::DictionaryValue& pc_dict,
                                const std::string& modifier,
                                const std::string& video_codec) {
  std::string story = video_codec.empty() ? "baseline_story" : video_codec;
  story += modifier;
  const base::DictionaryValue* stats_dict;
  ASSERT_TRUE(pc_dict.GetDictionary("stats", &stats_dict));
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  auto ssrc_iterator = ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    const std::string& ssrc = *ssrc_iterator;
    MaybePrintResultsForAudioReceive(ssrc, pc_dict, story);
    MaybePrintResultsForVideoReceive(ssrc, pc_dict, story);
  }
}

}  // namespace test
