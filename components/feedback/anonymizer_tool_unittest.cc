// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <gtest/gtest.h>

#include "base/strings/string_util.h"

namespace feedback {

const char kFakeFirstPartyID[] = "nkoccljplnhpfnfiajclkommnmllphnl";
const char* const kFakeFirstPartyExtensionIDs[] = {kFakeFirstPartyID, nullptr};

class AnonymizerToolTest : public testing::Test {
 protected:
  std::string AnonymizeMACAddresses(const std::string& input) {
    return anonymizer_.AnonymizeMACAddresses(input);
  }

  std::string AnonymizeCustomPatterns(const std::string& input) {
    return anonymizer_.AnonymizeCustomPatterns(input);
  }

  std::string AnonymizeCustomPatternWithContext(
      const std::string& input,
      const std::string& pattern,
      std::map<std::string, std::string>* space) {
    return anonymizer_.AnonymizeCustomPatternWithContext(input, pattern, space);
  }

  std::string AnonymizeCustomPatternWithoutContext(
      const std::string& input,
      const CustomPatternWithoutContext& pattern,
      std::map<std::string, std::string>* space) {
    return anonymizer_.AnonymizeCustomPatternWithoutContext(input, pattern,
                                                            space);
  }

  AnonymizerTool anonymizer_{kFakeFirstPartyExtensionIDs};
};

TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));

  // Make sure MAC address anonymization is invoked.
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8a:ce:13:57"));

  // Make sure custom pattern anonymization is invoked.
  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));

  // Make sure UUIDs are anonymized.
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/<UUID: 1> HTTP/1.1\" 200 291 "
      "Create-Job successful-ok",
      anonymizer_.Anonymize(
          "REQUEST localhost - - \"POST /printers/"
          "cb738a9f-6433-4d95-a81e-94e4ae0ed30b HTTP/1.1\" 200 291 Create-Job "
          "successful-ok"));
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/<UUID: 2> HTTP/1.1\" 200 286 "
      "Create-Job successful-ok",
      anonymizer_.Anonymize(
          "REQUEST localhost - - \"POST /printers/"
          "d17188da-9cd3-44f4-b148-3e1d748a3b0f HTTP/1.1\" 200 286 Create-Job "
          "successful-ok"));
}

TEST_F(AnonymizerToolTest, AnonymizeMACAddresses) {
  EXPECT_EQ("", AnonymizeMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", AnonymizeMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("aa:bb:cc:00:00:01", AnonymizeMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ(
      "BSSID: aa:bb:cc:00:00:01 in the middle\n"
      "bb:cc:dd:00:00:02 start of line\n"
      "end of line aa:bb:cc:00:00:01\n"
      "no match across lines aa:bb:cc:\n"
      "dd:ee:ff two on the same line:\n"
      "x bb:cc:dd:00:00:02 cc:dd:ee:00:00:03 x\n",
      AnonymizeMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                            "bb:cc:dd:ee:ff:00 start of line\n"
                            "end of line aa:bb:cc:dd:ee:ff\n"
                            "no match across lines aa:bb:cc:\n"
                            "dd:ee:ff two on the same line:\n"
                            "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember bb:cc:dd:00:00:02?",
            AnonymizeMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatterns) {
  EXPECT_EQ("", AnonymizeCustomPatterns(""));

  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '2'", AnonymizeCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '1' bar",
            AnonymizeCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '1' bar",
            AnonymizeCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='1' b\n'",
            AnonymizeCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '2'", AnonymizeCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", AnonymizeCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): 1\nfoo",
            AnonymizeCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ(
      "a\nb [SSID=1] [SSID=2] [SSID=foo\nbar] b",
      AnonymizeCustomPatterns("a\nb [SSID=foo] [SSID=bar] [SSID=foo\nbar] b"));

  EXPECT_EQ("SerialNumber: 1",
            AnonymizeCustomPatterns("SerialNumber: 1217D7EF"));
  EXPECT_EQ("serial  number: 2",
            AnonymizeCustomPatterns("serial  number: 50C971FEE7F3x010900"));
  EXPECT_EQ("SerialNumber: 3",
            AnonymizeCustomPatterns("SerialNumber: EVT23-17BA01-004"));
  EXPECT_EQ("serial=4", AnonymizeCustomPatterns("serial=\"1234AA5678\""));

  EXPECT_EQ("\"gaia_id\":\"1\"",
            AnonymizeCustomPatterns("\"gaia_id\":\"1234567890\""));
  EXPECT_EQ("gaia_id='2'", AnonymizeCustomPatterns("gaia_id='987654321'"));
  EXPECT_EQ("{id: 1, email:",
            AnonymizeCustomPatterns("{id: 123454321, email:"));

  EXPECT_EQ("<email: 1>",
            AnonymizeCustomPatterns("foo@bar.com"));
  EXPECT_EQ("Email: <email: 1>.",
            AnonymizeCustomPatterns("Email: foo@bar.com."));
  EXPECT_EQ("Email:\n<email: 2>\n",
            AnonymizeCustomPatterns("Email:\nfooooo@bar.com\n"));

  EXPECT_EQ("[<IPv6: 1>]", AnonymizeCustomPatterns(
                               "[2001:0db8:0000:0000:0000:ff00:0042:8329]"));
  EXPECT_EQ("[<IPv6: 2>]",
            AnonymizeCustomPatterns("[2001:db8:0:0:0:ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 3>]", AnonymizeCustomPatterns("[2001:db8::ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 4>]", AnonymizeCustomPatterns("[aa::bb]"));
  EXPECT_EQ("<IPv4: 1>", AnonymizeCustomPatterns("192.160.0.1"));

  EXPECT_EQ("<URL: 1>",
            AnonymizeCustomPatterns("http://example.com/foo?test=1"));
  EXPECT_EQ("Foo <URL: 2> Bar",
            AnonymizeCustomPatterns("Foo http://192.168.0.1/foo?test=1#123 Bar"));
  const char* kURLs[] = {
    "http://example.com/foo?test=1",
    "http://userid:password@example.com:8080",
    "http://userid:password@example.com:8080/",
    "http://@example.com",
    "http://192.168.0.1",
    "http://192.168.0.1/",
    "http://اختبار.com",
    "http://test.com/foo(bar)baz.html",
    "http://test.com/foo%20bar",
    "ftp://test:tester@test.com",
    "chrome://extensions/",
    "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/options.html",
    "http://example.com/foo?email=foo@bar.com",
    "rtsp://root@example.com/",
    "https://aaaaaaaaaaaaaaaa.com",
  };
  for (size_t i = 0; i < base::size(kURLs); ++i) {
    SCOPED_TRACE(kURLs[i]);
    std::string got = AnonymizeCustomPatterns(kURLs[i]);
    EXPECT_TRUE(
        base::StartsWith(got, "<URL: ", base::CompareCase::INSENSITIVE_ASCII));
    EXPECT_TRUE(base::EndsWith(got, ">", base::CompareCase::INSENSITIVE_ASCII));
  }
  // Test that "Android:" is not considered a schema with empty hier part.
  EXPECT_EQ("The following applies to Android:",
            AnonymizeCustomPatterns("The following applies to Android:"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatternWithContext) {
  const char kPattern[] = "(\\b(?i)id:? ')(\\d+)(')";
  std::map<std::string, std::string> space;
  EXPECT_EQ("", AnonymizeCustomPatternWithContext("", kPattern, &space));
  EXPECT_EQ("foo\nbar\n",
            AnonymizeCustomPatternWithContext("foo\nbar\n", kPattern, &space));
  EXPECT_EQ("id '1'",
            AnonymizeCustomPatternWithContext("id '2345'", kPattern, &space));
  EXPECT_EQ("id '2'",
            AnonymizeCustomPatternWithContext("id '1234'", kPattern, &space));
  EXPECT_EQ("id: '2'",
            AnonymizeCustomPatternWithContext("id: '1234'", kPattern, &space));
  EXPECT_EQ("ID: '1'",
            AnonymizeCustomPatternWithContext("ID: '2345'", kPattern, &space));
  EXPECT_EQ("x1 id '1' 1x id '2'\nid '1'\n",
            AnonymizeCustomPatternWithContext(
                "x1 id '2345' 1x id '1234'\nid '2345'\n", kPattern, &space));
  space.clear();
  EXPECT_EQ("id '1'",
            AnonymizeCustomPatternWithContext("id '1234'", kPattern, &space));

  space.clear();
  EXPECT_EQ("x1z",
            AnonymizeCustomPatternWithContext("xyz", "()(y+)()", &space));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatternWithoutContext) {
  CustomPatternWithoutContext kPattern = {"pattern", "(o+)"};
  std::map<std::string, std::string> space;
  EXPECT_EQ("", AnonymizeCustomPatternWithoutContext("", kPattern, &space));
  EXPECT_EQ("f<pattern: 1>\nf<pattern: 2>z\nf<pattern: 1>l\n",
            AnonymizeCustomPatternWithoutContext("fo\nfooz\nfol\n", kPattern,
                                                 &space));
}

TEST_F(AnonymizerToolTest, AnonymizeChunk) {
  // For better readability, put all the pre/post redaction strings in an array
  // of pairs, and then convert that to two strings which become the input and
  // output of the anonymizer.
  std::pair<std::string, std::string> data[] = {
      {"aaaaaaaa [SSID=123aaaaaa]aaaaa",  // SSID.
       "aaaaaaaa [SSID=1]aaaaa"},
      {"aaaaaaaahttp://tets.comaaaaaaa",  // URL.
       "aaaaaaaa<URL: 1>"},
      {"aaaaaemail@example.comaaa",  // Email address.
       "<email: 1>"},
      {"example@@1234",  // No PII, it is not invalid email address.
       "example@@1234"},
      {"255.255.155.2",  // IP address.
       "<IPv4: 1>"},
      {"255.255.155.255",  // IP address.
       "<IPv4: 2>"},
      {"127.0.0.1",  // IPv4 loopback.
       "<127.0.0.0/8: 3>"},
      {"127.255.0.1",  // IPv4 loopback.
       "<127.0.0.0/8: 4>"},
      {"0.0.0.0",  // Any IPv4.
       "<0.0.0.0/8: 5>"},
      {"0.255.255.255",  // Any IPv4.
       "<0.0.0.0/8: 6>"},
      {"10.10.10.100",  // IPv4 private class A.
       "<10.0.0.0/8: 7>"},
      {"10.10.10.100",  // Intentional duplicate.
       "<10.0.0.0/8: 7>"},
      {"10.10.10.101",  // IPv4 private class A.
       "<10.0.0.0/8: 8>"},
      {"10.255.255.255",  // IPv4 private class A.
       "<10.0.0.0/8: 9>"},
      {"172.16.0.0",  // IPv4 private class B.
       "<172.16.0.0/12: 10>"},
      {"172.31.255.255",  // IPv4 private class B.
       "<172.16.0.0/12: 11>"},
      {"172.11.5.5",  // IP address.
       "<IPv4: 12>"},
      {"172.111.5.5",  // IP address.
       "<IPv4: 13>"},
      {"192.168.0.0",  // IPv4 private class C.
       "<192.168.0.0/16: 14>"},
      {"192.168.255.255",  // IPv4 private class C.
       "<192.168.0.0/16: 15>"},
      {"192.169.2.120",  // IP address.
       "<IPv4: 16>"},
      {"169.254.0.1",  // Link local.
       "<169.254.0.0/16: 17>"},
      {"169.200.0.1",  // IP address.
       "<IPv4: 18>"},
      {"fe80::",  // Link local.
       "<fe80::/10: 1>"},
      {"fe80::ffff",  // Link local.
       "<fe80::/10: 2>"},
      {"febf:ffff::ffff",  // Link local.
       "<fe80::/10: 3>"},
      {"fecc::1111",  // IP address.
       "<IPv6: 4>"},
      {"224.0.0.24",  // Multicast.
       "<224.0.0.0/4: 19>"},
      {"240.0.0.0",  // IP address.
       "<IPv4: 20>"},
      {"255.255.255.255",  // Broadcast.
       "255.255.255.255"},
      {"100.115.92.92",  // ChromeOS.
       "100.115.92.92"},
      {"100.115.91.92",  // IP address.
       "<IPv4: 23>"},
      {"1.1.1.1",  // DNS
       "1.1.1.1"},
      {"8.8.8.8",  // DNS
       "8.8.8.8"},
      {"8.8.4.4",  // DNS
       "8.8.4.4"},
      {"8.8.8.4",  // IP address.
       "<IPv4: 27>"},
      {"255.255.259.255",  // Not an IP address.
       "255.255.259.255"},
      {"255.300.255.255",  // Not an IP address.
       "255.300.255.255"},
      {"aaaa123.123.45.4aaa",  // IP address.
       "aaaa<IPv4: 28>aaa"},
      {"11:11;11::11",  // IP address.
       "11:11;<IPv6: 5>"},
      {"11::11",  // IP address.
       "<IPv6: 5>"},
      {"11:11:abcdef:0:0:0:0:0",  // No PII.
       "11:11:abcdef:0:0:0:0:0"},
      {"::",  // Unspecified.
       "::"},
      {"::1",  // Local host.
       "::1"},
      {"Instance::Set",  // Ignore match, no PII.
       "Instance::Set"},
      {"Instant::ff",  // Ignore match, no PII.
       "Instant::ff"},
      {"net::ERR_CONN_TIMEOUT",  // Ignore match, no PII.
       "net::ERR_CONN_TIMEOUT"},
      {"ff01::1",  // All nodes address (interface local).
       "ff01::1"},
      {"ff01::2",  // All routers (interface local).
       "ff01::2"},
      {"ff01::3",  // Multicast (interface local).
       "<ff01::/16: 13>"},
      {"ff02::1",  // All nodes address (link local).
       "ff02::1"},
      {"ff02::2",  // All routers (link local).
       "ff02::2"},
      {"ff02::3",  // Multicast (link local).
       "<ff02::/16: 16>"},
      {"ff02::fb",  // mDNSv6 (link local).
       "<ff02::/16: 17>"},
      {"ff08::fb",  // mDNSv6.
       "<IPv6: 18>"},
      {"ff0f::101",  // All NTP servers.
       "<IPv6: 19>"},
      {"::ffff:cb0c:10ea",  // IPv4-mapped IPV6 (IP address).
       "<IPv6: 20>"},
      {"::ffff:a0a:a0a",  // IPv4-mapped IPV6 (private class A).
       "<M 10.0.0.0/8: 21>"},
      {"::ffff:a0a:a0a",  // Intentional duplicate.
       "<M 10.0.0.0/8: 21>"},
      {"::ffff:ac1e:1e1e",  // IPv4-mapped IPV6 (private class B).
       "<M 172.16.0.0/12: 22>"},
      {"::ffff:c0a8:640a",  // IPv4-mapped IPV6 (private class C).
       "<M 192.168.0.0/16: 23>"},
      {"::ffff:6473:5c01",  // IPv4-mapped IPV6 (Chrome).
       "<M 100.115.92.1: 24>"},
      {"64:ff9b::a0a:a0a",  // IPv4-translated 6to4 IPV6 (private class A).
       "<T 10.0.0.0/8: 25>"},
      {"64:ff9b::6473:5c01",  // IPv4-translated 6to4 IPV6 (Chrome).
       "<T 100.115.92.1: 26>"},
      {"::0101:ffff:c0a8:640a",  // IP address.
       "<IPv6: 27>"},
      {"aa:aa:aa:aa:aa:aa",  // MAC address (BSSID).
       "aa:aa:aa:00:00:01"},
      {"chrome://resources/foo",  // Secure chrome resource, whitelisted.
       "chrome://resources/foo"},
      {"chrome://settings/crisper.js",  // Whitelisted settings URLs.
       "chrome://settings/crisper.js"},
      // Whitelisted first party extension.
      {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js",
       "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js"},
      {"chrome://resources/f?user=bar",  // Potentially PII in parameter.
       "<URL: 2>"},
      {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x",
       "<URL: 3>"},  // Potentially PII in parameter.
  };
  std::string anon_input;
  std::string anon_output;
  for (const auto& s : data) {
    anon_input.append(s.first).append("\n");
    anon_output.append(s.second).append("\n");
  }
  EXPECT_EQ(anon_output, anonymizer_.Anonymize(anon_input));
}

}  // namespace feedback
