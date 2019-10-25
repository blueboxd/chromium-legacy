// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"

#include "components/security_interstitials/core/controller_client.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace security_interstitials {

gin::WrapperInfo SecurityInterstitialPageController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

SecurityInterstitialPageController::Delegate::~Delegate() {}

void SecurityInterstitialPageController::Install(
    content::RenderFrame* render_frame,
    base::WeakPtr<Delegate> delegate) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<SecurityInterstitialPageController> controller =
      gin::CreateHandle(isolate,
                        new SecurityInterstitialPageController(delegate));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "certificateErrorPageController"),
            controller.ToV8())
      .Check();
}

SecurityInterstitialPageController::SecurityInterstitialPageController(
    base::WeakPtr<Delegate> delegate)
    : delegate_(delegate) {}

SecurityInterstitialPageController::~SecurityInterstitialPageController() {}

void SecurityInterstitialPageController::DontProceed() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DONT_PROCEED);
}

void SecurityInterstitialPageController::Proceed() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::CMD_PROCEED);
}

void SecurityInterstitialPageController::ShowMoreSection() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::
                  CMD_SHOW_MORE_SECTION);
}

void SecurityInterstitialPageController::OpenHelpCenter() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::
                  CMD_OPEN_HELP_CENTER);
}

void SecurityInterstitialPageController::OpenDiagnostic() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_DIAGNOSTIC);
}

void SecurityInterstitialPageController::Reload() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::CMD_RELOAD);
}

void SecurityInterstitialPageController::OpenDateSettings() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::
                  CMD_OPEN_DATE_SETTINGS);
}

void SecurityInterstitialPageController::OpenLogin() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_LOGIN);
}

void SecurityInterstitialPageController::DoReport() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DO_REPORT);
}

void SecurityInterstitialPageController::DontReport() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_DONT_REPORT);
}

void SecurityInterstitialPageController::OpenReportingPrivacy() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::
                  CMD_OPEN_REPORTING_PRIVACY);
}

void SecurityInterstitialPageController::OpenWhitepaper() {
  SendCommand(
      security_interstitials::SecurityInterstitialCommand::CMD_OPEN_WHITEPAPER);
}

void SecurityInterstitialPageController::ReportPhishingError() {
  SendCommand(security_interstitials::SecurityInterstitialCommand::
                  CMD_REPORT_PHISHING_ERROR);
}

void SecurityInterstitialPageController::SendCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  if (delegate_) {
    delegate_->SendCommand(command);
  }
}

gin::ObjectTemplateBuilder
SecurityInterstitialPageController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SecurityInterstitialPageController>::
      GetObjectTemplateBuilder(isolate)
          .SetMethod("dontProceed",
                     &SecurityInterstitialPageController::DontProceed)
          .SetMethod("proceed", &SecurityInterstitialPageController::Proceed)
          .SetMethod("showMoreSection",
                     &SecurityInterstitialPageController::ShowMoreSection)
          .SetMethod("openHelpCenter",
                     &SecurityInterstitialPageController::OpenHelpCenter)
          .SetMethod("openDiagnostic",
                     &SecurityInterstitialPageController::OpenDiagnostic)
          .SetMethod("reload", &SecurityInterstitialPageController::Reload)
          .SetMethod("openDateSettings",
                     &SecurityInterstitialPageController::OpenDateSettings)
          .SetMethod("openLogin",
                     &SecurityInterstitialPageController::OpenLogin)
          .SetMethod("doReport", &SecurityInterstitialPageController::DoReport)
          .SetMethod("dontReport",
                     &SecurityInterstitialPageController::DontReport)
          .SetMethod("openReportingPrivacy",
                     &SecurityInterstitialPageController::OpenReportingPrivacy)
          .SetMethod("openWhitepaper",
                     &SecurityInterstitialPageController::OpenWhitepaper)
          .SetMethod("reportPhishingError",
                     &SecurityInterstitialPageController::ReportPhishingError);
}

}  // namespace security_interstitials
