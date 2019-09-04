// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_handler_host.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/native_error_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace payments {
namespace {

content::DevToolsBackgroundServicesContext* GetDevTools(
    content::BrowserContext* browser_context,
    const url::Origin& sw_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* storage_partition = content::BrowserContext::GetStoragePartitionForSite(
      browser_context, sw_origin.GetURL(), /*can_create=*/true);
  if (!storage_partition)
    return nullptr;

  auto* dev_tools = storage_partition->GetDevToolsBackgroundServicesContext();
  return dev_tools && dev_tools->IsRecording(
                          content::DevToolsBackgroundService::kPaymentHandler)
             ? dev_tools
             : nullptr;
}

}  // namespace

PaymentHandlerHost::PaymentHandlerHost(content::WebContents* web_contents,
                                       Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {
  DCHECK(web_contents_);
  DCHECK(delegate_);
}

PaymentHandlerHost::~PaymentHandlerHost() {}

mojo::PendingRemote<mojom::PaymentHandlerHost> PaymentHandlerHost::Bind() {
  receiver_.reset();
  mojo::PendingRemote<mojom::PaymentHandlerHost> host =
      receiver_.BindNewPipeAndPassRemote();

  // Connection error handler can be set only after the Bind() call.
  receiver_.set_disconnect_handler(base::BindOnce(
      &PaymentHandlerHost::Disconnect, weak_ptr_factory_.GetWeakPtr()));

  return host;
}

void PaymentHandlerHost::UpdateWith(
    mojom::PaymentMethodChangeResponsePtr response) {
  if (!change_payment_method_callback_)
    return;

  auto* dev_tools =
      GetDevTools(web_contents_->GetBrowserContext(), sw_origin_for_logs_);
  if (dev_tools) {
    std::map<std::string, std::string> data = {{"Error", response->error}};

    if (response->total) {
      data["Total Currency"] = response->total->currency;
      data["Total Value"] = response->total->value;
    }

    if (response->stringified_payment_method_errors) {
      data["Payment Method Errors"] =
          *response->stringified_payment_method_errors;
    }

    if (response->modifiers) {
      for (size_t i = 0; i < response->modifiers->size(); ++i) {
        std::string prefix =
            "Modifier" + (response->modifiers->size() == 1
                              ? ""
                              : " #" + base::NumberToString(i));
        const auto& modifier = response->modifiers->at(i);
        data.emplace(prefix + " Method Name",
                     modifier->method_data->method_name);
        data.emplace(prefix + " Method Data",
                     modifier->method_data->stringified_data);
        if (!modifier->total)
          continue;
        data.emplace(prefix + " Total Currency", modifier->total->currency);
        data.emplace(prefix + " Total Value", modifier->total->value);
      }
    }

    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_, sw_origin_for_logs_,
        content::DevToolsBackgroundService::kPaymentHandler, "Update with",
        /*instance_id=*/payment_request_id_for_logs_, data);
  }

  std::move(change_payment_method_callback_).Run(std::move(response));
}

void PaymentHandlerHost::NoUpdatedPaymentDetails() {
  if (!change_payment_method_callback_)
    return;

  std::move(change_payment_method_callback_)
      .Run(mojom::PaymentMethodChangeResponse::New());
}

void PaymentHandlerHost::Disconnect() {
  receiver_.reset();
}

base::WeakPtr<PaymentHandlerHost> PaymentHandlerHost::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentHandlerHost::ChangePaymentMethod(
    mojom::PaymentHandlerMethodDataPtr method_data,
    mojom::PaymentHandlerHost::ChangePaymentMethodCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!method_data) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kMethodDataRequired;
    std::move(callback).Run(std::move(response));
    return;
  }

  if (method_data->method_name.empty()) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kMethodNameRequired;
    std::move(callback).Run(std::move(response));
    return;
  }

  if (!delegate_->ChangePaymentMethod(method_data->method_name,
                                      method_data->stringified_data)) {
    mojom::PaymentMethodChangeResponsePtr response =
        mojom::PaymentMethodChangeResponse::New();
    response->error = errors::kInvalidState;
    std::move(callback).Run(std::move(response));
    return;
  }

  auto* dev_tools =
      GetDevTools(web_contents_->GetBrowserContext(), sw_origin_for_logs_);
  if (dev_tools) {
    dev_tools->LogBackgroundServiceEvent(
        registration_id_for_logs_, sw_origin_for_logs_,
        content::DevToolsBackgroundService::kPaymentHandler,
        "Change payment method",
        /*instance_id=*/payment_request_id_for_logs_,
        {{"Method Name", method_data->method_name},
         {"Method Data", method_data->stringified_data}});
  }

  change_payment_method_callback_ = std::move(callback);
}

}  // namespace payments
