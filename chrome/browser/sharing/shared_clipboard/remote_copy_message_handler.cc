// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync/protocol/sharing_remote_copy_message.pb.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "skia/ext/image_operations.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/origin.h"

namespace {
constexpr size_t kMaxImageDownloadSize = 5 * 1024 * 1024;

// These values are the 2x of the preferred width and height defined in
// message_center_constants.h, which are in dip.
constexpr int kNotificationImageMaxWidthPx = 720;
constexpr int kNotificationImageMaxHeightPx = 480;

// This method should be called on a ThreadPool thread because it performs a
// potentially slow operation.
SkBitmap ResizeImage(const SkBitmap& image, int width, int height) {
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, width, height);
}

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_copy_message_handler",
                                        R"(
          semantics {
            sender: "RemoteCopyMessageHandler"
            description:
              "Fetches an image from a URL specified in an FCM message."
            trigger:
              "The user sent an image to this device from another device that "
              "they control."
            data:
              "An image URL, from a Google storage service like blobstore."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can disable this behavior by signing out of Chrome."
            policy_exception_justification:
              "Can be controlled via Chrome sign-in."
          })");

base::string16 GetTextNotificationTitle(const std::string& device_name) {
  return device_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT_UNKNOWN_DEVICE)
             : l10n_util::GetStringFUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                   base::UTF8ToUTF16(device_name));
}

base::string16 GetImageNotificationTitle(const std::string& device_name) {
  return device_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT_UNKNOWN_DEVICE)
             : l10n_util::GetStringFUTF16(
                   IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT,
                   base::UTF8ToUTF16(device_name));
}
}  // namespace

RemoteCopyMessageHandler::RemoteCopyMessageHandler(Profile* profile)
    : profile_(profile) {}

RemoteCopyMessageHandler::~RemoteCopyMessageHandler() = default;

void RemoteCopyMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    DoneCallback done_callback) {
  DCHECK(message.has_remote_copy_message());

  // First cancel any pending async tasks that might otherwise overwrite the
  // results of the more recent message.
  url_loader_.reset();
  ImageDecoder::Cancel(this);
  resize_callback_.Cancel();

  device_name_ = message.sender_device_name();

  switch (message.remote_copy_message().content_case()) {
    case chrome_browser_sharing::RemoteCopyMessage::kText:
      HandleText(message.remote_copy_message().text());
      break;
    case chrome_browser_sharing::RemoteCopyMessage::kImageUrl:
      HandleImage(message.remote_copy_message().image_url());
      break;
    case chrome_browser_sharing::RemoteCopyMessage::CONTENT_NOT_SET:
      NOTREACHED();
      break;
  }

  std::move(done_callback).Run(/*response=*/nullptr);
}

void RemoteCopyMessageHandler::HandleText(const std::string& text) {
  if (text.empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureEmptyText);
    return;
  }

  LogRemoteCopyReceivedTextSize(text.size());

  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));
  }
  ShowNotification(GetTextNotificationTitle(device_name_), SkBitmap());
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledText);
}

void RemoteCopyMessageHandler::HandleImage(const std::string& image_url) {
  GURL url(image_url);

  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageUrlNotTrustworthy);
    return;
  }

  if (!IsOriginAllowed(url)) {
    Finish(RemoteCopyHandleMessageResult::kFailureImageOriginNotAllowed);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  // This request should be unauthenticated (no cookies), and shouldn't be
  // stored in the cache (this URL is only fetched once, ever.)
  request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  timer_ = base::ElapsedTimer();
  // Unretained(this) is safe here because |this| owns |url_loader_|.
  url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&RemoteCopyMessageHandler::OnURLLoadComplete,
                     base::Unretained(this)),
      kMaxImageDownloadSize);
}

bool RemoteCopyMessageHandler::IsOriginAllowed(const GURL& image_url) {
  url::Origin image_origin = url::Origin::Create(image_url);
  std::vector<std::string> parts =
      base::SplitString(kRemoteCopyAllowedOrigins.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& part : parts) {
    url::Origin allowed_origin = url::Origin::Create(GURL(part));
    if (image_origin.IsSameOriginWith(allowed_origin))
      return true;
  }
  return false;
}

void RemoteCopyMessageHandler::OnURLLoadComplete(
    std::unique_ptr<std::string> content) {
  int code;
  if (url_loader_->NetError() != net::OK) {
    code = url_loader_->NetError();
  } else if (!url_loader_->ResponseInfo() ||
             !url_loader_->ResponseInfo()->headers) {
    code = net::OK;
  } else {
    code = url_loader_->ResponseInfo()->headers->response_code();
  }
  LogRemoteCopyLoadImageStatusCode(code);

  url_loader_.reset();
  if (!content || content->empty()) {
    Finish(RemoteCopyHandleMessageResult::kFailureNoImageContentLoaded);
    return;
  }

  LogRemoteCopyLoadImageTime(timer_.Elapsed());
  LogRemoteCopyReceivedImageSizeBeforeDecode(content->size());

  timer_ = base::ElapsedTimer();
  ImageDecoder::Start(this, *content);
}

void RemoteCopyMessageHandler::OnImageDecoded(const SkBitmap& image) {
  if (image.drawsNothing()) {
    Finish(RemoteCopyHandleMessageResult::kFailureDecodedImageDrawsNothing);
    return;
  }

  LogRemoteCopyDecodeImageTime(timer_.Elapsed());
  LogRemoteCopyReceivedImageSizeAfterDecode(image.computeByteSize());

  double scale = std::min(
      static_cast<double>(kNotificationImageMaxWidthPx) / image.width(),
      static_cast<double>(kNotificationImageMaxHeightPx) / image.height());

  // If the image is too large to show in a notification, resize it first.
  if (scale < 1.0) {
    int resized_width =
        base::ClampToRange(static_cast<int>(scale * image.width()), 0,
                           kNotificationImageMaxWidthPx);
    int resized_height =
        base::ClampToRange(static_cast<int>(scale * image.height()), 0,
                           kNotificationImageMaxHeightPx);

    // Unretained(this) is safe here because |this| owns |resize_callback_|.
    resize_callback_.Reset(
        base::BindOnce(&RemoteCopyMessageHandler::WriteImageAndShowNotification,
                       base::Unretained(this), image));
    timer_ = base::ElapsedTimer();
    base::PostTaskAndReplyWithResult(
        FROM_HERE, {base::ThreadPool(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ResizeImage, image, resized_width, resized_height),
        resize_callback_.callback());
  } else {
    WriteImageAndShowNotification(image, image);
  }
}

void RemoteCopyMessageHandler::OnDecodeImageFailed() {
  Finish(RemoteCopyHandleMessageResult::kFailureDecodeImageFailed);
}

void RemoteCopyMessageHandler::WriteImageAndShowNotification(
    const SkBitmap& original_image,
    const SkBitmap& resized_image) {
  if (original_image.dimensions() != resized_image.dimensions())
    LogRemoteCopyResizeImageTime(timer_.Elapsed());

  {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteImage(original_image);
  }

  ShowNotification(GetImageNotificationTitle(device_name_), resized_image);
  Finish(RemoteCopyHandleMessageResult::kSuccessHandledImage);
}

void RemoteCopyMessageHandler::ShowNotification(const base::string16& title,
                                                const SkBitmap& image) {
  std::string notification_id = base::GenerateGUID();

  message_center::RichNotificationData rich_notification_data;
  if (!image.drawsNothing())
    rich_notification_data.image = gfx::Image::CreateFrom1xBitmap(image);
  rich_notification_data.vector_small_image = &kSendTabToSelfIcon;

  message_center::NotificationType type =
      image.drawsNothing() ? message_center::NOTIFICATION_TYPE_SIMPLE
                           : message_center::NOTIFICATION_TYPE_IMAGE;

  message_center::Notification notification(
      type, notification_id, title,
      l10n_util::GetStringUTF16(
          IDS_SHARING_REMOTE_COPY_NOTIFICATION_DESCRIPTION),
      /*icon=*/gfx::Image(),
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      rich_notification_data,
      /*delegate=*/nullptr);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SHARING, notification, /*metadata=*/nullptr);
}

void RemoteCopyMessageHandler::Finish(RemoteCopyHandleMessageResult result) {
  LogRemoteCopyHandleMessageResult(result);
  device_name_.clear();
}
