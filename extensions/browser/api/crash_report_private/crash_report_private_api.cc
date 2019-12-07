// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/crash_report_private/crash_report_private_api.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "components/crash/content/app/client_upload_info.h"
#include "components/feedback/anonymizer_tool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/api/crash_report_private.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace extensions {
namespace api {

namespace {

// Used for throttling the API calls.
base::Time g_last_called_time;

base::Clock* g_clock = base::DefaultClock::GetInstance();

#if defined(GOOGLE_CHROME_BUILD)
constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";
#else
constexpr char kCrashEndpointUrl[] = "";
#endif

std::string& GetCrashEndpoint() {
  static base::NoDestructor<std::string> crash_endpoint(kCrashEndpointUrl);
  return *crash_endpoint;
}

constexpr int kCrashEndpointResponseMaxSizeInBytes = 1024;

void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       base::OnceCallback<void()> callback,
                       std::unique_ptr<std::string> response_body) {
  if (response_body) {
    DVLOG(1) << "Uploaded crash report. ID: " << *response_body;
  } else {
    LOG(ERROR) << "Failed to upload crash report";
  }
  std::move(callback).Run();
}

// Sometimes, the stack trace will contain an error message as the first line,
// which confuses the Crash server. This function deletes it if it is present.
std::string RemoveErrorMessageFromStackTrace(const std::string& stack_trace,
                                             const std::string& error_message) {
  // Return the original stack trace if the error message is not present.
  const auto error_message_index = stack_trace.find(error_message);
  if (error_message_index == std::string::npos)
    return stack_trace;

  // If the stack trace only contains one line, then delete the whole trace.
  const auto first_line_end_index = stack_trace.find('\n');
  if (first_line_end_index == std::string::npos)
    return std::string();

  // Otherwise, delete the first line.
  return stack_trace.substr(first_line_end_index + 1);
}

using ParameterMap = std::map<std::string, std::string>;

std::string BuildPostRequestQueryString(const ParameterMap& params) {
  std::vector<std::string> query_parts;
  for (const auto& kv : params) {
    query_parts.push_back(base::StrCat(
        {kv.first, "=",
         net::EscapeQueryParamValue(kv.second, /* use_plus */ false)}));
  }
  return base::JoinString(query_parts, "&");
}

struct PlatformInfo {
  std::string product_name;
  std::string version;
  std::string channel;
  std::string platform;
  std::string os_version;
};

PlatformInfo GetPlatformInfo() {
  PlatformInfo info;
  crash_reporter::GetClientProductNameAndVersion(&info.product_name,
                                                 &info.version, &info.channel);

  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

  info.os_version = base::StringPrintf("%d.%d.%d", os_major_version,
                                       os_minor_version, os_bugfix_version);
  return info;
}

void SendReport(scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
                const GURL& url,
                const std::string& body,
                base::OnceCallback<void()> callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = url;

  const auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("javascript_report_error", R"(
      semantics {
        sender: "JavaScript error reporter"
        description:
          "Chrome can send JavaScript errors that occur within built-in "
          "component extensions. If enabled, the error message, along "
          "with information about Chrome and the operating system."
        trigger:
            "A JavaScript error occurs in a Chrome component extension"
        data:
            "The JavaScript error message, the version and channel of Chrome, "
            "the URL of the extension, the line and column number where the "
            "error occurred, and a stack trace of the error."
        destination: GOOGLE_OWNED_SERVICE
      }
  )");

  DVLOG(1) << "Sending crash report: " << resource_request->url;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (!body.empty()) {
    url_loader->AttachStringForUpload(body, "text/plain");
  }

  network::SimpleURLLoader* loader = url_loader.get();
  loader->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&OnRequestComplete, std::move(url_loader),
                     std::move(callback)),
      kCrashEndpointResponseMaxSizeInBytes);
}

void ReportJavaScriptError(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const crash_report_private::ErrorInfo& error,
    base::OnceCallback<void()> callback,
    const std::string& anonymized_message) {
  const auto platform = GetPlatformInfo();

  const GURL source(error.url);
  const auto product = error.product ? *error.product : platform.product_name;
  const auto version = error.version ? *error.version : platform.version;

  ParameterMap params;
  params["prod"] = net::EscapeQueryParamValue(product, /* use_plus */ false);
  params["ver"] = net::EscapeQueryParamValue(version, /* use_plus */ false);
  params["type"] = "JavascriptError";
  params["error_message"] = anonymized_message;
  params["browser"] = "Chrome";
  params["browser_version"] = platform.version;
  params["channel"] = platform.channel;
  params["os"] = "ChromeOS";
  params["os_version"] = platform.os_version;
  params["full_url"] = source.spec();
  params["url"] = source.path();
  params["src"] = source.spec();
  if (error.line_number)
    params["line"] = *error.line_number;
  if (error.column_number)
    params["column"] = *error.column_number;

  const GURL url(base::StrCat(
      {GetCrashEndpoint(), "?", BuildPostRequestQueryString(params)}));
  const std::string body =
      error.stack_trace
          ? RemoveErrorMessageFromStackTrace(*error.stack_trace, error.message)
          : "";

  SendReport(loader_factory, url, body, std::move(callback));
}

std::string AnonymizeErrorMessage(const std::string& message) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return feedback::AnonymizerTool(/*first_party_extension_ids=*/nullptr)
      .Anonymize(message);
}

}  // namespace

CrashReportPrivateReportErrorFunction::CrashReportPrivateReportErrorFunction() =
    default;

CrashReportPrivateReportErrorFunction::
    ~CrashReportPrivateReportErrorFunction() = default;

ExtensionFunction::ResponseAction CrashReportPrivateReportErrorFunction::Run() {
  // Do not report errors if the user did not give consent for crash reporting.
  if (!crash_reporter::GetClientCollectStatsConsent())
    return RespondNow(NoArguments());

  // Ensure we don't send too many crash reports. Limit to one report per hour.
  if (!g_last_called_time.is_null() &&
      g_clock->Now() - g_last_called_time < base::TimeDelta::FromHours(1)) {
    return RespondNow(Error("Too many calls to this API"));
  }

  // TODO(https://crbug.com/986166): Use crash_reporter for Chrome OS.
  const auto params = crash_report_private::ReportError::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetURLLoaderFactoryForBrowserProcess();

  // Don't anonymize the report on the UI thread as it can take some time.
  PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&AnonymizeErrorMessage, params->info.message),
      base::BindOnce(
          &ReportJavaScriptError, std::move(loader_factory),
          std::move(params->info),
          base::BindOnce(
              &CrashReportPrivateReportErrorFunction::OnReportComplete, this)));
  g_last_called_time = base::Time::Now();

  return RespondLater();
}

void CrashReportPrivateReportErrorFunction::OnReportComplete() {
  Respond(NoArguments());
}

void SetClockForTesting(base::Clock* clock) {
  g_clock = clock;
}

void SetCrashEndpointForTesting(const std::string& endpoint) {
  GetCrashEndpoint() = endpoint;
}

}  // namespace api
}  // namespace extensions
