// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/anonymizer_tool.h"

#include <gtest/gtest.h>

#include "base/strings/string_util.h"

namespace feedback {

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

  AnonymizerTool anonymizer_;
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
  std::string data =
      "aaaaaaaa [SSID=123aaaaaa]aaaaa\n"  // SSID.
      "aaaaaaaahttp://tets.comaaaaaaa\n"  // URL.
      "aaaaaemail@example.comaaa\n"       // Email address.
      "example@@1234\n"           // No PII, it is not valid email address.
      "255.255.155.2\n"           // IP address.
      "255.255.155.255\n"         // IP address.
      "127.0.0.1\n"               // IPv4 loopback.
      "127.255.0.1\n"             // IPv4 loopback.
      "0.0.0.0\n"                 // Any IPv4.
      "0.255.255.255\n"           // Any IPv4.
      "10.10.10.100\n"            // IPv4 private class A.
      "10.10.10.100\n"            // Intentional duplicate.
      "10.10.10.101\n"            // IPv4 private class A.
      "10.255.255.255\n"          // IPv4 private class A.
      "172.16.0.0\n"              // IPv4 private class B.
      "172.31.255.255\n"          // IPv4 private class B.
      "172.11.5.5\n"              // IP address.
      "172.111.5.5\n"             // IP address.
      "192.168.0.0\n"             // IPv4 private class C.
      "192.168.255.255\n"         // IPv4 private class C.
      "192.169.2.120\n"           // IP address.
      "169.254.0.1\n"             // Link local.
      "169.200.0.1\n"             // IP address.
      "fe80::\n"                  // Link local.
      "fe80::ffff\n"              // Link local.
      "febf:ffff::ffff\n"         // Link local.
      "fecc::1111\n"              // IP address.
      "224.0.0.24\n"              // Multicast.
      "240.0.0.0\n"               // IP address.
      "255.255.255.255\n"         // Broadcast.
      "100.115.92.92\n"           // ChromeOS.
      "100.115.91.92\n"           // IP address.
      "1.1.1.1\n"                 // DNS
      "8.8.8.8\n"                 // DNS
      "8.8.4.4\n"                 // DNS
      "8.8.8.4\n"                 // IP address.
      "255.255.259.255\n"         // Not an IP address.
      "255.300.255.255\n"         // Not an IP address.
      "aaaa123.123.45.4aaa\n"     // IP address.
      "11:11;11::11\n"            // IP address.
      "11::11\n"                  // IP address.
      "11:11:abcdef:0:0:0:0:0\n"  // No PII.
      "::\n"                      // Unspecified.
      "::1\n"                     // Local host.
      "Instance::Set\n"           // Ignore match, no PII.
      "Instant::ff\n"             // Ignore match, no PII.
      "net::ERR_CONN_TIMEOUT\n"   // Ignore match, no PII.
      "ff01::1\n"                 // All nodes address (interface local).
      "ff01::2\n"                 // All routers (interface local).
      "ff01::3\n"                 // Multicast (interface local).
      "ff02::1\n"                 // All nodes address (link local).
      "ff02::2\n"                 // All routers (link local).
      "ff02::3\n"                 // Multicast (link local).
      "ff02::fb\n"                // mDNSv6 (link local).
      "ff08::fb\n"                // mDNSv6.
      "ff0f::101\n"               // All NTP servers.
      "::ffff:cb0c:10ea\n"        // IPv4-mapped IPV6 (IP address).
      "::ffff:a0a:a0a\n"          // IPv4-mapped IPV6 (private class A).
      "::ffff:a0a:a0a\n"          // Intentional duplicate.
      "::ffff:ac1e:1e1e\n"        // IPv4-mapped IPV6 (private class B).
      "::ffff:c0a8:640a\n"        // IPv4-mapped IPV6 (private class C).
      "::ffff:6473:5c01\n"        // IPv4-mapped IPV6 (Chrome).
      "64:ff9b::a0a:a0a\n"       // IPv4-translated 6to4 IPV6 (private class A).
      "64:ff9b::6473:5c01\n"     // IPv4-translated 6to4 IPV6 (Chrome).
      "::0101:ffff:c0a8:640a\n"  // IP address.
      "aa:aa:aa:aa:aa:aa\n"      // MAC address (BSSID).
      "chrome://resources/foo\n"        // Secure chrome resource, whitelisted.
      "chrome://resources/f?user=bar";  // Potentially PII in parameter.
  std::string result =
      "aaaaaaaa [SSID=1]aaaaa\n"
      "aaaaaaaa<URL: 1>\n"
      "<email: 1>\n"
      "example@@1234\n"
      "<IPv4: 1>\n"
      "<IPv4: 2>\n"
      "<127.0.0.0/8: 3>\n"
      "<127.0.0.0/8: 4>\n"
      "<0.0.0.0/8: 5>\n"
      "<0.0.0.0/8: 6>\n"
      "<10.0.0.0/8: 7>\n"
      "<10.0.0.0/8: 7>\n"
      "<10.0.0.0/8: 8>\n"
      "<10.0.0.0/8: 9>\n"
      "<172.16.0.0/12: 10>\n"
      "<172.16.0.0/12: 11>\n"
      "<IPv4: 12>\n"
      "<IPv4: 13>\n"
      "<192.168.0.0/16: 14>\n"
      "<192.168.0.0/16: 15>\n"
      "<IPv4: 16>\n"
      "<169.254.0.0/16: 17>\n"
      "<IPv4: 18>\n"
      "<fe80::/10: 1>\n"
      "<fe80::/10: 2>\n"
      "<fe80::/10: 3>\n"
      "<IPv6: 4>\n"
      "<224.0.0.0/4: 19>\n"
      "<IPv4: 20>\n"
      "255.255.255.255\n"
      "100.115.92.92\n"
      "<IPv4: 23>\n"
      "1.1.1.1\n"
      "8.8.8.8\n"
      "8.8.4.4\n"
      "<IPv4: 27>\n"
      "255.255.259.255\n"
      "255.300.255.255\n"
      "aaaa<IPv4: 28>aaa\n"
      "11:11;<IPv6: 5>\n"
      "<IPv6: 5>\n"
      "11:11:abcdef:0:0:0:0:0\n"
      "::\n"
      "::1\n"
      "Instance::Set\n"
      "Instant::ff\n"
      "net::ERR_CONN_TIMEOUT\n"
      "ff01::1\n"
      "ff01::2\n"
      "<ff01::/16: 13>\n"
      "ff02::1\n"
      "ff02::2\n"
      "<ff02::/16: 16>\n"
      "<ff02::/16: 17>\n"
      "<IPv6: 18>\n"
      "<IPv6: 19>\n"
      "<IPv6: 20>\n"
      "<M 10.0.0.0/8: 21>\n"
      "<M 10.0.0.0/8: 21>\n"
      "<M 172.16.0.0/12: 22>\n"
      "<M 192.168.0.0/16: 23>\n"
      "<M 100.115.92.1: 24>\n"
      "<T 10.0.0.0/8: 25>\n"
      "<T 100.115.92.1: 26>\n"
      "<IPv6: 27>\n"
      "aa:aa:aa:00:00:01\n"
      "chrome://resources/foo\n"
      "<URL: 2>";
  EXPECT_EQ(result, anonymizer_.Anonymize(data));
}

}  // namespace feedback
