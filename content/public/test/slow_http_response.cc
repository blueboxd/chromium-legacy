// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/slow_http_response.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

namespace {

static bool g_should_finish_response = false;

void SendResponseBodyDone(const net::test_server::SendBytesCallback& send,
                          net::test_server::SendCompleteCallback done);

// The response body is sent in two parts, of size |kFirstResponsePartSize| and
// |kSecondResponsePartSize| respectively.
void SendResponseBody(const net::test_server::SendBytesCallback& send,
                      net::test_server::SendCompleteCallback done,
                      bool finish_response) {
  int data_size = finish_response ? SlowHttpResponse::kSecondResponsePartSize
                                  : SlowHttpResponse::kFirstResponsePartSize;
  std::string response(data_size, '*');

  if (finish_response) {
    send.Run(response, std::move(done));
  } else {
    send.Run(response,
             base::BindOnce(&SendResponseBodyDone, send, std::move(done)));
  }
}

// Called when the response body was successfully sent.
void SendResponseBodyDone(const net::test_server::SendBytesCallback& send,
                          net::test_server::SendCompleteCallback done) {
  if (g_should_finish_response) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SendResponseBody, send, std::move(done), true),
        base::TimeDelta::FromMilliseconds(100));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&SendResponseBodyDone, send, std::move(done)),
        base::TimeDelta::FromMilliseconds(100));
  }
}

}  // namespace

// static
const char SlowHttpResponse::kSlowResponseHostName[] =
    "url.handled.by.slow.response";
const char SlowHttpResponse::kSlowResponseUrl[] = "/slow-response";
const char SlowHttpResponse::kFinishSlowResponseUrl[] = "/slow-response-finish";

const int SlowHttpResponse::kFirstResponsePartSize = 1024 * 35;
const int SlowHttpResponse::kSecondResponsePartSize = 1024 * 10;

SlowHttpResponse::SlowHttpResponse(const std::string& url) : url_(url) {}

SlowHttpResponse::~SlowHttpResponse() = default;

bool SlowHttpResponse::IsHandledUrl() {
  return url_ == kSlowResponseUrl || url_ == kFinishSlowResponseUrl;
}

void SlowHttpResponse::AddResponseHeaders(std::string* response) {
  response->append("Content-type: text/html\r\n");
}

void SlowHttpResponse::SetStatusLine(std::string* response) {
  response->append("HTTP/1.1 200 OK\r\n");
}

void SlowHttpResponse::SendResponse(
    const net::test_server::SendBytesCallback& send,
    net::test_server::SendCompleteCallback done) {
  std::string response;
  SetStatusLine(&response);
  if (base::LowerCaseEqualsASCII(kFinishSlowResponseUrl, url_)) {
    response.append("Content-type: text/plain\r\n");
    response.append("\r\n");

    g_should_finish_response = true;
    send.Run(response, std::move(done));
  } else {
    AddResponseHeaders(&response);
    response.append("Cache-Control: max-age=0\r\n");
    response.append("\r\n");
    send.Run(response,
             base::BindOnce(&SendResponseBody, send, std::move(done), false));
  }
}

}  // namespace content
