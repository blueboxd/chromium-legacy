// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/child/child_thread_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/child_process.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

#if defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#else
#include "content/child/webthemeengine_impl_default.h"
#endif

#if defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#endif

using blink::WebData;
using blink::WebLocalizedString;
using blink::WebString;
using blink::WebThemeEngine;
using blink::WebURL;
using blink::WebURLError;

namespace content {

namespace {

std::unique_ptr<blink::WebThemeEngine> GetWebThemeEngine() {
#if defined(OS_ANDROID)
  return std::make_unique<WebThemeEngineAndroid>();
#elif defined(OS_MACOSX)
  if (features::IsFormControlsRefreshEnabled())
    return std::make_unique<WebThemeEngineDefault>();
  return std::make_unique<WebThemeEngineMac>();
#else
  return std::make_unique<WebThemeEngineDefault>();
#endif
}

int ToMessageID(int resource_id) {
  switch (resource_id) {
    case WebLocalizedString::kAXAMPMFieldText:
      return IDS_AX_AM_PM_FIELD_TEXT;
    case WebLocalizedString::kAXCalendarShowDatePicker:
      return IDS_AX_CALENDAR_SHOW_DATE_PICKER;
    case WebLocalizedString::kAXCalendarShowMonthSelector:
      return IDS_AX_CALENDAR_SHOW_MONTH_SELECTOR;
    case WebLocalizedString::kAXCalendarShowNextMonth:
      return IDS_AX_CALENDAR_SHOW_NEXT_MONTH;
    case WebLocalizedString::kAXCalendarShowPreviousMonth:
      return IDS_AX_CALENDAR_SHOW_PREVIOUS_MONTH;
    case WebLocalizedString::kAXCalendarWeekDescription:
      return IDS_AX_CALENDAR_WEEK_DESCRIPTION;
    case WebLocalizedString::kAXDayOfMonthFieldText:
      return IDS_AX_DAY_OF_MONTH_FIELD_TEXT;
    case WebLocalizedString::kAXHourFieldText:
      return IDS_AX_HOUR_FIELD_TEXT;
    case WebLocalizedString::kAXMediaDefault:
      return IDS_AX_MEDIA_DEFAULT;
    case WebLocalizedString::kAXMediaAudioElement:
      return IDS_AX_MEDIA_AUDIO_ELEMENT;
    case WebLocalizedString::kAXMediaVideoElement:
      return IDS_AX_MEDIA_VIDEO_ELEMENT;
    case WebLocalizedString::kAXMediaMuteButton:
      return IDS_AX_MEDIA_MUTE_BUTTON;
    case WebLocalizedString::kAXMediaUnMuteButton:
      return IDS_AX_MEDIA_UNMUTE_BUTTON;
    case WebLocalizedString::kAXMediaPlayButton:
      return IDS_AX_MEDIA_PLAY_BUTTON;
    case WebLocalizedString::kAXMediaPauseButton:
      return IDS_AX_MEDIA_PAUSE_BUTTON;
    case WebLocalizedString::kAXMediaCurrentTimeDisplay:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY;
    case WebLocalizedString::kAXMediaTimeRemainingDisplay:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY;
    case WebLocalizedString::kAXMediaEnterFullscreenButton:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON;
    case WebLocalizedString::kAXMediaExitFullscreenButton:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON;
    case WebLocalizedString::kAXMediaDisplayCutoutFullscreenButton:
      return IDS_AX_MEDIA_DISPLAY_CUT_OUT_FULL_SCREEN_BUTTON;
    case WebLocalizedString::kAXMediaEnterPictureInPictureButton:
      return IDS_AX_MEDIA_ENTER_PICTURE_IN_PICTURE_BUTTON;
    case WebLocalizedString::kAXMediaExitPictureInPictureButton:
      return IDS_AX_MEDIA_EXIT_PICTURE_IN_PICTURE_BUTTON;
    case WebLocalizedString::kAXMediaShowClosedCaptionsMenuButton:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_MENU_BUTTON;
    case WebLocalizedString::kAXMediaHideClosedCaptionsMenuButton:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_MENU_BUTTON;
    case WebLocalizedString::kAXMediaLoadingPanel:
      return IDS_AX_MEDIA_LOADING_PANEL;
    case WebLocalizedString::kAXMediaCastOffButton:
      return IDS_AX_MEDIA_CAST_OFF_BUTTON;
    case WebLocalizedString::kAXMediaCastOnButton:
      return IDS_AX_MEDIA_CAST_ON_BUTTON;
    case WebLocalizedString::kAXMediaDownloadButton:
      return IDS_AX_MEDIA_DOWNLOAD_BUTTON;
    case WebLocalizedString::kAXMediaOverflowButton:
      return IDS_AX_MEDIA_OVERFLOW_BUTTON;
    case WebLocalizedString::kAXMediaAudioElementHelp:
      return IDS_AX_MEDIA_AUDIO_ELEMENT_HELP;
    case WebLocalizedString::kAXMediaVideoElementHelp:
      return IDS_AX_MEDIA_VIDEO_ELEMENT_HELP;
    case WebLocalizedString::kAXMediaAudioSliderHelp:
      return IDS_AX_MEDIA_AUDIO_SLIDER_HELP;
    case WebLocalizedString::kAXMediaVideoSliderHelp:
      return IDS_AX_MEDIA_VIDEO_SLIDER_HELP;
    case WebLocalizedString::kAXMediaVolumeSliderHelp:
      return IDS_AX_MEDIA_VOLUME_SLIDER_HELP;
    case WebLocalizedString::kAXMediaCurrentTimeDisplayHelp:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY_HELP;
    case WebLocalizedString::kAXMediaTimeRemainingDisplayHelp:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY_HELP;
    case WebLocalizedString::kAXMediaOverflowButtonHelp:
      return IDS_AX_MEDIA_OVERFLOW_BUTTON_HELP;
    case WebLocalizedString::kAXMediaTouchLessPlayPauseAction:
      return IDS_AX_MEDIA_TOUCHLESS_PLAY_PAUSE_ACTION;
    case WebLocalizedString::kAXMediaTouchLessSeekAction:
      return IDS_AX_MEDIA_TOUCHLESS_SEEK_ACTION;
    case WebLocalizedString::kAXMediaTouchLessVolumeAction:
      return IDS_AX_MEDIA_TOUCHLESS_VOLUME_ACTION;
    case WebLocalizedString::kAXMillisecondFieldText:
      return IDS_AX_MILLISECOND_FIELD_TEXT;
    case WebLocalizedString::kAXMinuteFieldText:
      return IDS_AX_MINUTE_FIELD_TEXT;
    case WebLocalizedString::kAXMonthFieldText:
      return IDS_AX_MONTH_FIELD_TEXT;
    case WebLocalizedString::kAXSecondFieldText:
      return IDS_AX_SECOND_FIELD_TEXT;
    case WebLocalizedString::kAXWeekOfYearFieldText:
      return IDS_AX_WEEK_OF_YEAR_FIELD_TEXT;
    case WebLocalizedString::kAXYearFieldText:
      return IDS_AX_YEAR_FIELD_TEXT;
    case WebLocalizedString::kCalendarClear:
      return IDS_FORM_CALENDAR_CLEAR;
    case WebLocalizedString::kCalendarToday:
      return IDS_FORM_CALENDAR_TODAY;
    case WebLocalizedString::kDetailsLabel:
      return IDS_DETAILS_WITHOUT_SUMMARY_LABEL;
    case WebLocalizedString::kFileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::kFileButtonChooseMultipleFilesLabel:
      return IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL;
    case WebLocalizedString::kFileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::kInputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::kMissingPluginText:
      return IDS_PLUGIN_INITIALIZATION_ERROR;
    case WebLocalizedString::kAXMediaPlaybackError:
      return IDS_MEDIA_PLAYBACK_ERROR;
    case WebLocalizedString::kMediaRemotingCastText:
      return IDS_MEDIA_REMOTING_CAST_TEXT;
    case WebLocalizedString::kMediaRemotingCastToUnknownDeviceText:
      return IDS_MEDIA_REMOTING_CAST_TO_UNKNOWN_DEVICE_TEXT;
    case WebLocalizedString::kMediaRemotingStopByErrorText:
      return IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT;
    case WebLocalizedString::kMediaRemotingStopByPlaybackQualityText:
      return IDS_MEDIA_REMOTING_STOP_BY_PLAYBACK_QUALITY_TEXT;
    case WebLocalizedString::kMediaRemotingStopNoText:
      return -1;  // This string name is used only to indicate an empty string.
    case WebLocalizedString::kMediaRemotingStopText:
      return IDS_MEDIA_REMOTING_STOP_TEXT;
    case WebLocalizedString::kMediaScrubbingMessageText:
      return IDS_MEDIA_SCRUBBING_MESSAGE_TEXT;
    case WebLocalizedString::kMultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::kOtherColorLabel:
      return IDS_FORM_OTHER_COLOR_LABEL;
    case WebLocalizedString::kOtherDateLabel:
      return IDS_FORM_OTHER_DATE_LABEL;
    case WebLocalizedString::kOtherMonthLabel:
      return IDS_FORM_OTHER_MONTH_LABEL;
    case WebLocalizedString::kOtherWeekLabel:
      return IDS_FORM_OTHER_WEEK_LABEL;
    case WebLocalizedString::kOverflowMenuCaptions:
      return IDS_MEDIA_OVERFLOW_MENU_CLOSED_CAPTIONS;
    case WebLocalizedString::kOverflowMenuCaptionsSubmenuTitle:
      return IDS_MEDIA_OVERFLOW_MENU_CLOSED_CAPTIONS_SUBMENU_TITLE;
    case WebLocalizedString::kOverflowMenuCast:
      return IDS_MEDIA_OVERFLOW_MENU_CAST;
    case WebLocalizedString::kOverflowMenuEnterFullscreen:
      return IDS_MEDIA_OVERFLOW_MENU_ENTER_FULLSCREEN;
    case WebLocalizedString::kOverflowMenuExitFullscreen:
      return IDS_MEDIA_OVERFLOW_MENU_EXIT_FULLSCREEN;
    case WebLocalizedString::kOverflowMenuMute:
      return IDS_MEDIA_OVERFLOW_MENU_MUTE;
    case WebLocalizedString::kOverflowMenuUnmute:
      return IDS_MEDIA_OVERFLOW_MENU_UNMUTE;
    case WebLocalizedString::kOverflowMenuPlay:
      return IDS_MEDIA_OVERFLOW_MENU_PLAY;
    case WebLocalizedString::kOverflowMenuPause:
      return IDS_MEDIA_OVERFLOW_MENU_PAUSE;
    case WebLocalizedString::kOverflowMenuDownload:
      return IDS_MEDIA_OVERFLOW_MENU_DOWNLOAD;
    case WebLocalizedString::kOverflowMenuEnterPictureInPicture:
      return IDS_MEDIA_OVERFLOW_MENU_ENTER_PICTURE_IN_PICTURE;
    case WebLocalizedString::kOverflowMenuExitPictureInPicture:
      return IDS_MEDIA_OVERFLOW_MENU_EXIT_PICTURE_IN_PICTURE;
    case WebLocalizedString::kPictureInPictureInterstitialText:
      return IDS_MEDIA_PICTURE_IN_PICTURE_INTERSTITIAL_TEXT;
    case WebLocalizedString::kPlaceholderForDayOfMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_DAY_OF_MONTH_FIELD;
    case WebLocalizedString::kPlaceholderForMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_MONTH_FIELD;
    case WebLocalizedString::kPlaceholderForYearField:
      return IDS_FORM_PLACEHOLDER_FOR_YEAR_FIELD;
    case WebLocalizedString::kResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::kSelectMenuListText:
      return IDS_FORM_SELECT_MENU_LIST_TEXT;
    case WebLocalizedString::kSubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::kThisMonthButtonLabel:
      return IDS_FORM_THIS_MONTH_LABEL;
    case WebLocalizedString::kThisWeekButtonLabel:
      return IDS_FORM_THIS_WEEK_LABEL;
    case WebLocalizedString::kValidationBadInputForDateTime:
      return IDS_FORM_VALIDATION_BAD_INPUT_DATETIME;
    case WebLocalizedString::kValidationBadInputForNumber:
      return IDS_FORM_VALIDATION_BAD_INPUT_NUMBER;
    case WebLocalizedString::kValidationPatternMismatch:
      return IDS_FORM_VALIDATION_PATTERN_MISMATCH;
    case WebLocalizedString::kValidationRangeOverflow:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW;
    case WebLocalizedString::kValidationRangeOverflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW_DATETIME;
    case WebLocalizedString::kValidationRangeUnderflow:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW;
    case WebLocalizedString::kValidationRangeUnderflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW_DATETIME;
    case WebLocalizedString::kValidationStepMismatch:
      return IDS_FORM_VALIDATION_STEP_MISMATCH;
    case WebLocalizedString::kValidationStepMismatchCloseToLimit:
      return IDS_FORM_VALIDATION_STEP_MISMATCH_CLOSE_TO_LIMIT;
    case WebLocalizedString::kValidationTooLong:
      return IDS_FORM_VALIDATION_TOO_LONG;
    case WebLocalizedString::kValidationTooShort:
      return IDS_FORM_VALIDATION_TOO_SHORT;
    case WebLocalizedString::kValidationTooShortPlural:
      return IDS_FORM_VALIDATION_TOO_SHORT_PLURAL;
    case WebLocalizedString::kValidationTypeMismatch:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH;
    case WebLocalizedString::kValidationTypeMismatchForEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmpty:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmptyDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_DOMAIN;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmptyLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_LOCAL;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOMAIN;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidDots:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOTS;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_LOCAL;
    case WebLocalizedString::kValidationTypeMismatchForEmailNoAtSign:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_NO_AT_SIGN;
    case WebLocalizedString::kValidationTypeMismatchForMultipleEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL;
    case WebLocalizedString::kValidationTypeMismatchForURL:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_URL;
    case WebLocalizedString::kValidationValueMissing:
      return IDS_FORM_VALIDATION_VALUE_MISSING;
    case WebLocalizedString::kValidationValueMissingForCheckbox:
      return IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX;
    case WebLocalizedString::kValidationValueMissingForFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_FILE;
    case WebLocalizedString::kValidationValueMissingForMultipleFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE;
    case WebLocalizedString::kValidationValueMissingForRadio:
      return IDS_FORM_VALIDATION_VALUE_MISSING_RADIO;
    case WebLocalizedString::kValidationValueMissingForSelect:
      return IDS_FORM_VALIDATION_VALUE_MISSING_SELECT;
    case WebLocalizedString::kWeekFormatTemplate:
      return IDS_FORM_INPUT_WEEK_TEMPLATE;
    case WebLocalizedString::kWeekNumberLabel:
      return IDS_FORM_WEEK_NUMBER_LABEL;
    case WebLocalizedString::kTextTracksNoLabel:
      return IDS_MEDIA_TRACKS_NO_LABEL;
    case WebLocalizedString::kTextTracksOff:
      return IDS_MEDIA_TRACKS_OFF;
    case WebLocalizedString::kUnitsKibibytes:
      return IDS_UNITS_KIBIBYTES;
    case WebLocalizedString::kUnitsMebibytes:
      return IDS_UNITS_MEBIBYTES;
    case WebLocalizedString::kUnitsGibibytes:
      return IDS_UNITS_GIBIBYTES;
    case WebLocalizedString::kUnitsTebibytes:
      return IDS_UNITS_TEBIBYTES;
    case WebLocalizedString::kUnitsPebibytes:
      return IDS_UNITS_PEBIBYTES;
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

// This must match third_party/WebKit/public/blink_resources.grd.
// In particular, |is_gzipped| corresponds to compress="gzip".
struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
  bool is_gzipped;
};

class NestedMessageLoopRunnerImpl
    : public blink::Platform::NestedMessageLoopRunner {
 public:
  NestedMessageLoopRunnerImpl() = default;

  ~NestedMessageLoopRunnerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::RunLoop* const previous_run_loop = run_loop_;
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = previous_run_loop;
  }

  void QuitNow() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

 private:
  base::RunLoop* run_loop_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

mojo::SharedRemote<mojom::ChildProcessHost> GetChildProcessHost() {
  auto* thread = ChildThreadImpl::current();
  if (thread)
    return thread->child_process_host();
  return {};
}

// An implementation of BrowserInterfaceBroker which forwards to the
// ChildProcessHost interface. This lives on the IO thread.
class BrowserInterfaceBrokerProxyImpl
    : public blink::ThreadSafeBrowserInterfaceBrokerProxy {
 public:
  BrowserInterfaceBrokerProxyImpl() : process_host_(GetChildProcessHost()) {}

  // blink::ThreadSafeBrowserInterfaceBrokerProxy implementation:
  void GetInterfaceImpl(mojo::GenericPendingReceiver receiver) override {
    if (process_host_)
      process_host_->BindHostReceiver(std::move(receiver));
  }

 private:
  ~BrowserInterfaceBrokerProxyImpl() override = default;

  const mojo::SharedRemote<mojom::ChildProcessHost> process_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInterfaceBrokerProxyImpl);
};

}  // namespace

// TODO(skyostil): Ensure that we always have an active task runner when
// constructing the platform.
BlinkPlatformImpl::BlinkPlatformImpl()
    : BlinkPlatformImpl(base::ThreadTaskRunnerHandle::IsSet()
                            ? base::ThreadTaskRunnerHandle::Get()
                            : nullptr,
                        nullptr) {}

BlinkPlatformImpl::BlinkPlatformImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      io_thread_task_runner_(std::move(io_thread_task_runner)),
      browser_interface_broker_proxy_(
          base::MakeRefCounted<BrowserInterfaceBrokerProxyImpl>()),
      native_theme_engine_(GetWebThemeEngine()) {}

BlinkPlatformImpl::~BlinkPlatformImpl() = default;

void BlinkPlatformImpl::RecordAction(const blink::UserMetricsAction& name) {
  if (ChildThread* child_thread = ChildThread::Get())
    child_thread->RecordComputedAction(name.Action());
}

WebData BlinkPlatformImpl::GetDataResource(int resource_id,
                                           ui::ScaleFactor scale_factor) {
  base::StringPiece resource =
      GetContentClient()->GetDataResource(resource_id, scale_factor);
  return WebData(resource.data(), resource.size());
}

WebData BlinkPlatformImpl::UncompressDataResource(int resource_id) {
  base::StringPiece resource =
      GetContentClient()->GetDataResource(resource_id, ui::SCALE_FACTOR_NONE);
  if (resource.empty())
    return WebData(resource.data(), resource.size());
  std::string uncompressed;
  CHECK(compression::GzipUncompress(resource.as_string(), &uncompressed));
  return WebData(uncompressed.data(), uncompressed.size());
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id) {
  int message_id = ToMessageID(resource_id);
  if (message_id < 0)
    return WebString();
  return WebString::FromUTF16(
      GetContentClient()->GetLocalizedString(message_id));
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id,
                                                  const WebString& value) {
  int message_id = ToMessageID(resource_id);
  if (message_id < 0)
    return WebString();

  base::string16 format_string =
      GetContentClient()->GetLocalizedString(message_id);

  // If the ContentClient returned an empty string, e.g. because it's using the
  // default implementation of ContentClient::GetLocalizedString, return an
  // empty string instead of crashing with a failed DCHECK in
  // base::ReplaceStringPlaceholders below. This is useful for tests that don't
  // specialize a full ContentClient, since this way they can behave as though
  // there isn't a defined |message_id| for the |name| instead of crashing
  // outright.
  if (format_string.empty())
    return WebString();

  return WebString::FromUTF16(
      base::ReplaceStringPlaceholders(format_string, value.Utf16(), nullptr));
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id,
                                                  const WebString& value1,
                                                  const WebString& value2) {
  int message_id = ToMessageID(resource_id);
  if (message_id < 0)
    return WebString();
  std::vector<base::string16> values;
  values.reserve(2);
  values.push_back(value1.Utf16());
  values.push_back(value2.Utf16());
  return WebString::FromUTF16(base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), values, nullptr));
}

bool BlinkPlatformImpl::AllowScriptExtensionForServiceWorker(
    const blink::WebSecurityOrigin& script_origin) {
  return GetContentClient()->AllowScriptExtensionForServiceWorker(
      script_origin);
}

blink::WebCrypto* BlinkPlatformImpl::Crypto() {
  return &web_crypto_;
}

blink::ThreadSafeBrowserInterfaceBrokerProxy*
BlinkPlatformImpl::GetBrowserInterfaceBrokerProxy() {
  return browser_interface_broker_proxy_.get();
}

WebThemeEngine* BlinkPlatformImpl::ThemeEngine() {
  return native_theme_engine_.get();
}

bool BlinkPlatformImpl::IsURLSupportedForAppCache(const blink::WebURL& url) {
  return IsSchemeSupportedForAppCache(url);
}

size_t BlinkPlatformImpl::MaxDecodedImageBytes() {
  const int kMB = 1024 * 1024;
  const int kMaxNumberOfBytesPerPixel = 4;
#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    // Limit image decoded size to 3M pixels on low end devices.
    // 4 is maximum number of bytes per pixel.
    return 3 * kMB * kMaxNumberOfBytesPerPixel;
  }
  // For other devices, limit decoded image size based on the amount of physical
  // memory.
  // In some cases all physical memory is not accessible by Chromium, as it can
  // be reserved for direct use by certain hardware. Thus, we set the limit so
  // that 1.6GB of reported physical memory on a 2GB device is enough to set the
  // limit at 16M pixels, which is a desirable value since 4K*4K is a relatively
  // common texture size.
  return base::SysInfo::AmountOfPhysicalMemory() / 25;
#else
  size_t max_decoded_image_byte_limit = kNoDecodedImageByteLimit;
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kMaxDecodedImageSizeMb)) {
    if (base::StringToSizeT(
            command_line.GetSwitchValueASCII(switches::kMaxDecodedImageSizeMb),
            &max_decoded_image_byte_limit)) {
      max_decoded_image_byte_limit *= kMB * kMaxNumberOfBytesPerPixel;
    }
  }
  return max_decoded_image_byte_limit;
#endif
}

bool BlinkPlatformImpl::IsLowEndDevice() {
  return base::SysInfo::IsLowEndDevice();
}

scoped_refptr<base::SingleThreadTaskRunner> BlinkPlatformImpl::GetIOTaskRunner()
    const {
  return io_thread_task_runner_;
}

std::unique_ptr<blink::Platform::NestedMessageLoopRunner>
BlinkPlatformImpl::CreateNestedMessageLoopRunner() const {
  return std::make_unique<NestedMessageLoopRunnerImpl>();
}

}  // namespace content
