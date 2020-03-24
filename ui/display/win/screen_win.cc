// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/scaling_util.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace win {
namespace {

// TODO(robliao): http://crbug.com/615514 Remove when ScreenWin usage is
// resolved with Desktop Aura and WindowTreeHost.
ScreenWin* g_instance = nullptr;

// Gets the DPI for a particular monitor.
base::Optional<int> GetPerMonitorDPI(HMONITOR monitor) {
  if (!base::win::IsProcessPerMonitorDpiAware())
    return base::nullopt;

  static auto get_dpi_for_monitor_func = []() {
    const HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    return reinterpret_cast<decltype(&::GetDpiForMonitor)>(
        shcore_dll ? ::GetProcAddress(shcore_dll, "GetDpiForMonitor")
                   : nullptr);
  }();
  UINT dpi_x, dpi_y;
  if (!get_dpi_for_monitor_func ||
      !SUCCEEDED(
          get_dpi_for_monitor_func(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)))
    return base::nullopt;

  DCHECK_EQ(dpi_x, dpi_y);
  return int{dpi_x};
}

float GetScaleFactorForDPI(int dpi, bool include_accessibility) {
  const float scale = display::win::internal::GetScalingFactorFromDPI(dpi);
  return include_accessibility
             ? (scale * UwpTextScaleFactor::Instance()->GetTextScaleFactor())
             : scale;
}

// Gets the raw monitor scale factor.
//
// Respects the forced device scale factor, and will fall back to the global
// scale factor if per-monitor DPI is not supported.
float GetMonitorScaleFactor(HMONITOR monitor,
                            bool include_accessibility = true) {
  DCHECK(monitor);
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();

  const auto dpi = GetPerMonitorDPI(monitor);
  return dpi ? GetScaleFactorForDPI(dpi.value(), include_accessibility)
             : GetDPIScale();
}

std::vector<DISPLAYCONFIG_PATH_INFO> GetPathInfos() {
  for (LONG result = ERROR_INSUFFICIENT_BUFFER;
       result == ERROR_INSUFFICIENT_BUFFER;) {
    uint32_t path_elements, mode_elements;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                    &mode_elements) != ERROR_SUCCESS) {
      return {};
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> path_infos(path_elements);
    std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos(mode_elements);
    result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                path_infos.data(), &mode_elements,
                                mode_infos.data(), nullptr);
    if (result == ERROR_SUCCESS) {
      path_infos.resize(path_elements);
      return path_infos;
    }
  }
  return {};
}

base::Optional<DISPLAYCONFIG_PATH_INFO> GetPathInfo(HMONITOR monitor) {
  // Get the monitor name.
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!GetMonitorInfo(monitor, &monitor_info))
    return base::nullopt;

  // Look for a path info with a matching name.
  std::vector<DISPLAYCONFIG_PATH_INFO> path_infos = GetPathInfos();
  for (const auto& info : path_infos) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name = {};
    device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    device_name.header.size = sizeof(device_name);
    device_name.header.adapterId = info.sourceInfo.adapterId;
    device_name.header.id = info.sourceInfo.id;
    if ((DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) &&
        (wcscmp(monitor_info.szDevice, device_name.viewGdiDeviceName) == 0))
      return info;
  }
  return base::nullopt;
}

float GetMonitorSDRWhiteLevel(HMONITOR monitor) {
  if (auto path_info = GetPathInfo(monitor)) {
    DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
    white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    white_level.header.size = sizeof(white_level);
    white_level.header.adapterId = path_info->targetInfo.adapterId;
    white_level.header.id = path_info->targetInfo.id;
    if (DisplayConfigGetDeviceInfo(&white_level.header) == ERROR_SUCCESS)
      return white_level.SDRWhiteLevel * 80.0 / 1000.0;
  }
  return 200.0f;
}

Display::Rotation OrientationToRotation(DWORD orientation) {
  switch (orientation) {
    case DMDO_DEFAULT:
      return Display::ROTATE_0;
    case DMDO_90:
      return Display::ROTATE_90;
    case DMDO_180:
      return Display::ROTATE_180;
    case DMDO_270:
      return Display::ROTATE_270;
    default:
      NOTREACHED();
      return Display::ROTATE_0;
  }
}

struct DisplaySettings {
  Display::Rotation rotation;
  int frequency;
};
DisplaySettings GetDisplaySettingsForDevice(const wchar_t* device_name) {
  DEVMODE mode = {};
  mode.dmSize = sizeof(mode);
  if (!::EnumDisplaySettings(device_name, ENUM_CURRENT_SETTINGS, &mode))
    return {Display::ROTATE_0, 0};
  return {OrientationToRotation(mode.dmDisplayOrientation),
          mode.dmDisplayFrequency};
}

std::vector<DisplayInfo> FindAndRemoveTouchingDisplayInfos(
    const DisplayInfo& parent_info,
    std::vector<DisplayInfo>* display_infos) {
  std::vector<DisplayInfo> touching_display_infos;
  base::EraseIf(*display_infos, [&](const auto& display_info) {
    if (DisplayInfosTouch(parent_info, display_info)) {
      touching_display_infos.push_back(display_info);
      return true;
    }
    return false;
  });
  return touching_display_infos;
}

gfx::DisplayColorSpaces GetSourceColorSpaces(
    const Display& display,
    ColorProfileReader* color_profile_reader,
    bool hdr_enabled) {
  if (Display::HasForceDisplayColorProfile())
    return display.color_spaces();
  if (hdr_enabled)
    return gfx::DisplayColorSpaces();
  return gfx::DisplayColorSpaces(
      color_profile_reader->GetDisplayColorSpace(display.id()));
}

void ConfigureColorSpacesForHdr(float sdr_white_level,
                                gfx::DisplayColorSpaces* color_spaces) {
  color_spaces->SetSDRWhiteLevel(sdr_white_level);

  // This will map to DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709. In that space,
  // the brightness of (1,1,1) is 80 nits.
  constexpr float kScrgbWhiteLevel = 80.0f;
  const auto scrgb_linear =
      gfx::ColorSpace::CreateSCRGBLinear(kScrgbWhiteLevel / sdr_white_level);

  // This will map to DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, with sRGB's
  // (1,1,1) mapping to the specified number of nits.
  const auto hdr10 = gfx::ColorSpace::CreateHDR10(sdr_white_level);

  // Use HDR color spaces only when there is WCG or HDR content on the screen.
  constexpr bool kNeedsAlpha = true;
  for (const auto& usage : {gfx::ContentColorUsage::kWideColorGamut,
                            gfx::ContentColorUsage::kHDR}) {
    // Using RGBA F16 backbuffers required by SCRGB linear causes stuttering on
    // Windows RS3, but RGB10A2 with HDR10 color space works fine (see
    // https://crbug.com/937108#c92).
    if (base::win::GetVersion() > base::win::Version::WIN10_RS3) {
      color_spaces->SetOutputColorSpaceAndBufferFormat(
          usage, !kNeedsAlpha, scrgb_linear, gfx::BufferFormat::RGBA_F16);
    } else {
      color_spaces->SetOutputColorSpaceAndBufferFormat(
          usage, !kNeedsAlpha, hdr10, gfx::BufferFormat::BGRA_1010102);
    }
    // Use RGBA F16 backbuffers for HDR if alpha channel is required.
    color_spaces->SetOutputColorSpaceAndBufferFormat(
        usage, kNeedsAlpha, scrgb_linear, gfx::BufferFormat::RGBA_F16);
  }
}

Display CreateDisplayFromDisplayInfo(const DisplayInfo& display_info,
                                     ColorProfileReader* color_profile_reader,
                                     bool hdr_enabled) {
  const float scale_factor = display_info.device_scale_factor();
  const gfx::Rect bounds = gfx::ScaleToEnclosingRect(display_info.screen_rect(),
                                                     1.0f / scale_factor);
  Display display(display_info.id(), bounds);
  display.set_device_scale_factor(scale_factor);
  display.set_work_area(gfx::ScaleToEnclosingRect(
      display_info.screen_work_rect(), 1.0f / scale_factor));
  display.set_rotation(display_info.rotation());
  display.set_display_frequency(display_info.display_frequency());

  // Compute the DisplayColorSpace for this configuration.
  gfx::DisplayColorSpaces color_spaces =
      GetSourceColorSpaces(display, color_profile_reader, hdr_enabled);
  // When alpha is not needed, specify BGRX_8888 to get DXGI_ALPHA_MODE_IGNORE.
  // This saves significant power (see https://crbug.com/1057163).
  color_spaces.SetOutputBufferFormats(gfx::BufferFormat::BGRX_8888,
                                      gfx::BufferFormat::BGRA_8888);
  if (hdr_enabled && !Display::HasForceDisplayColorProfile()) {
    ConfigureColorSpacesForHdr(display_info.sdr_white_level(), &color_spaces);

    // These are (ab)used by pages via media query APIs to detect HDR support.
    display.set_color_depth(Display::kHDR10BitsPerPixel);
    display.set_depth_per_component(Display::kHDR10BitsPerComponent);
  }
  display.set_color_spaces(color_spaces);

  return display;
}

// Windows historically has had a hard time handling displays of DPIs higher
// than 96. Handling multiple DPI displays means we have to deal with Windows'
// monitor physical coordinates and map into Chrome's DIP coordinates.
//
// To do this, DisplayInfosToScreenWinDisplays reasons over monitors as a tree
// using the primary monitor as the root. All monitors touching this root are
// considered children.
//
// This also presumes that all monitors are connected components. By UI
// construction, Windows restricts the layout of monitors to connected
// components except when DPI virtualization is happening. When this happens, we
// scale relative to (0, 0).
//
// Note that this does not handle cases where a scaled display may have
// insufficient room to lay out its children. In these cases, a DIP point could
// map to multiple screen points due to overlap. The first discovered screen
// will take precedence.
std::vector<ScreenWinDisplay> DisplayInfosToScreenWinDisplays(
    const std::vector<DisplayInfo>& display_infos,
    ColorProfileReader* color_profile_reader,
    bool hdr_enabled) {
  // Find and extract the primary display.
  std::vector<DisplayInfo> display_infos_remaining = display_infos;
  auto primary_display_iter = std::find_if(
      display_infos_remaining.begin(), display_infos_remaining.end(),
      [](const DisplayInfo& display_info) {
        return display_info.screen_rect().origin().IsOrigin();
      });
  DCHECK(primary_display_iter != display_infos_remaining.end());

  // Build the tree and determine DisplayPlacements along the way.
  DisplayLayoutBuilder builder(primary_display_iter->id());
  std::vector<DisplayInfo> available_parents = {*primary_display_iter};
  display_infos_remaining.erase(primary_display_iter);
  while (!available_parents.empty()) {
    const DisplayInfo parent = available_parents.back();
    available_parents.pop_back();
    for (const auto& child :
         FindAndRemoveTouchingDisplayInfos(parent, &display_infos_remaining)) {
      builder.AddDisplayPlacement(CalculateDisplayPlacement(parent, child));
      available_parents.push_back(child);
    }
  }

  // Layout and create the ScreenWinDisplays.
  std::vector<Display> displays;
  for (const auto& display_info : display_infos) {
    displays.push_back(CreateDisplayFromDisplayInfo(
        display_info, color_profile_reader, hdr_enabled));
  }
  builder.Build()->ApplyToDisplayList(&displays, nullptr, 0);

  std::vector<ScreenWinDisplay> screen_win_displays;
  for (size_t i = 0; i < display_infos.size(); ++i)
    screen_win_displays.emplace_back(displays[i], display_infos[i]);
  return screen_win_displays;
}

std::vector<Display> ScreenWinDisplaysToDisplays(
    const std::vector<ScreenWinDisplay>& screen_win_displays) {
  std::vector<Display> displays;
  for (const auto& screen_win_display : screen_win_displays)
    displays.push_back(screen_win_display.display());
  return displays;
}

MONITORINFOEX MonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

base::Optional<gfx::Vector2dF> GetPixelsPerInchForPointerDevice(
    HANDLE source_device) {
  static const auto get_pointer_device_rects =
      reinterpret_cast<decltype(&::GetPointerDeviceRects)>(
          base::win::GetUser32FunctionPointer("GetPointerDeviceRects"));
  RECT device_rect, screen_rect;
  if (!get_pointer_device_rects ||
      !get_pointer_device_rects(source_device, &device_rect, &screen_rect))
    return base::nullopt;

  const gfx::RectF device{gfx::Rect(device_rect)};
  const gfx::RectF screen{gfx::Rect(screen_rect)};
  constexpr float kHimetricPerInch = 2540.0f;
  const float himetric_per_pixel_x = device.width() / screen.width();
  const float himetric_per_pixel_y = device.height() / screen.height();
  return gfx::Vector2dF(kHimetricPerInch / himetric_per_pixel_x,
                        kHimetricPerInch / himetric_per_pixel_y);
}

// Returns physical pixels per inch based on 96 dpi monitor.
gfx::Vector2dF GetDefaultMonitorPhysicalPixelsPerInch() {
  const int default_dpi = GetDPIFromScalingFactor(1.0f);
  return gfx::Vector2dF(default_dpi, default_dpi);
}

// Retrieves PPI for |monitor| based on touch pointer device handles.  Returns
// nullopt if a pointer device for |monitor| can't be found.
base::Optional<gfx::Vector2dF> GetMonitorPixelsPerInch(HMONITOR monitor) {
  static const auto get_pointer_devices =
      reinterpret_cast<decltype(&::GetPointerDevices)>(
          base::win::GetUser32FunctionPointer("GetPointerDevices"));
  uint32_t pointer_device_count = 0;
  if (!get_pointer_devices ||
      !get_pointer_devices(&pointer_device_count, nullptr) ||
      (pointer_device_count == 0))
    return base::nullopt;

  std::vector<POINTER_DEVICE_INFO> pointer_devices(pointer_device_count);
  if (!get_pointer_devices(&pointer_device_count, pointer_devices.data()))
    return base::nullopt;

  for (const auto& device : pointer_devices) {
    if (device.pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH &&
        device.monitor == monitor)
      return GetPixelsPerInchForPointerDevice(device.device);
  }
  return base::nullopt;
}

BOOL CALLBACK EnumMonitorForDisplayInfoCallback(HMONITOR monitor,
                                                HDC hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  const MONITORINFOEX monitor_info = MonitorInfoFromHMONITOR(monitor);
  const auto display_settings =
      GetDisplaySettingsForDevice(monitor_info.szDevice);
  const gfx::Vector2dF pixels_per_inch =
      GetMonitorPixelsPerInch(monitor).value_or(
          GetDefaultMonitorPhysicalPixelsPerInch());

  auto* display_infos = reinterpret_cast<std::vector<DisplayInfo>*>(data);
  DCHECK(display_infos);
  display_infos->emplace_back(monitor_info, GetMonitorScaleFactor(monitor),
                              GetMonitorSDRWhiteLevel(monitor),
                              display_settings.rotation,
                              display_settings.frequency, pixels_per_inch);
  return TRUE;
}

std::vector<DisplayInfo> GetDisplayInfosFromSystem() {
  std::vector<DisplayInfo> display_infos;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForDisplayInfoCallback,
                      reinterpret_cast<LPARAM>(&display_infos));
  DCHECK_EQ(::GetSystemMetrics(SM_CMONITORS), int{display_infos.size()});
  return display_infos;
}

// Returns |point|, transformed from |from_origin|'s to |to_origin|'s
// coordinates, which differ by |scale_factor|.
gfx::PointF ScalePointRelative(const gfx::PointF& point,
                               const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor) {
  const gfx::PointF relative_point = point - from_origin.OffsetFromOrigin();
  const gfx::PointF scaled_relative_point =
      gfx::ScalePoint(relative_point, scale_factor);
  return scaled_relative_point + to_origin.OffsetFromOrigin();
}

gfx::PointF ScreenToDIPPoint(const gfx::PointF& screen_point,
                             const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  return ScalePointRelative(
      screen_point, screen_win_display.pixel_bounds().origin(),
      display.bounds().origin(), 1.0f / display.device_scale_factor());
}

gfx::Point DIPToScreenPoint(const gfx::Point& dip_point,
                            const ScreenWinDisplay& screen_win_display) {
  const Display display = screen_win_display.display();
  return gfx::ToFlooredPoint(
      ScalePointRelative(gfx::PointF(dip_point), display.bounds().origin(),
                         screen_win_display.pixel_bounds().origin(),
                         display.device_scale_factor()));
}

}  // namespace

ScreenWin::ScreenWin() : ScreenWin(true) {}

ScreenWin::~ScreenWin() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
gfx::PointF ScreenWin::ScreenToDIPPoint(const gfx::PointF& pixel_point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestScreenPoint,
                             gfx::ToFlooredPoint(pixel_point));
  return display::win::ScreenToDIPPoint(pixel_point, screen_win_display);
}

// static
gfx::Point ScreenWin::DIPToScreenPoint(const gfx::Point& dip_point) {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestDIPPoint, dip_point);
  return display::win::DIPToScreenPoint(dip_point, screen_win_display);
}

// static
gfx::Point ScreenWin::ClientToDIPPoint(HWND hwnd,
                                       const gfx::Point& client_point) {
  return ScaleToFlooredPoint(client_point, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Point ScreenWin::DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point) {
  return ScaleToFlooredPoint(dip_point, GetScaleFactorForHWND(hwnd));
}

// static
gfx::Rect ScreenWin::ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestScreenRect, pixel_bounds);
  const gfx::Point origin = gfx::ToFlooredPoint(display::win::ScreenToDIPPoint(
      gfx::PointF(pixel_bounds.origin()), screen_win_display));
  const float scale_factor =
      1.0f / screen_win_display.display().device_scale_factor();
  return {origin, ScaleToEnclosingRect(pixel_bounds, scale_factor).size()};
}

// static
gfx::Rect ScreenWin::DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestDIPRect, dip_bounds);
  const gfx::Point origin =
      display::win::DIPToScreenPoint(dip_bounds.origin(), screen_win_display);
  const float scale_factor = screen_win_display.display().device_scale_factor();
  return {origin, ScaleToEnclosingRect(dip_bounds, scale_factor).size()};
}

// static
gfx::Rect ScreenWin::ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  return ScaleToEnclosingRect(pixel_bounds, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Rect ScreenWin::DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  return ScaleToEnclosingRect(dip_bounds, GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::ScreenToDIPSize(HWND hwnd,
                                     const gfx::Size& size_in_pixels) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(size_in_pixels, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(dip_size, GetScaleFactorForHWND(hwnd));
}

// static
int ScreenWin::GetSystemMetricsForMonitor(HMONITOR monitor, int metric) {
  if (!g_instance)
    return ::GetSystemMetrics(metric);

  // Fall back to the primary display's HMONITOR.
  if (!monitor)
    monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

  // We don't include fudge factors stemming from accessiblility features when
  // dealing with system metrics associated with window elements drawn by the
  // operating system, since we will not be doing scaling of those metrics
  // ourselves.
  const bool include_accessibility = (metric != SM_CXSIZEFRAME) &&
                                     (metric != SM_CYSIZEFRAME) &&
                                     (metric != SM_CXPADDEDBORDER);

  // We'll then pull up the system metrics scaled by the appropriate amount.
  return g_instance->GetSystemMetricsForScaleFactor(
      GetMonitorScaleFactor(monitor, include_accessibility), metric);
}

// static
int ScreenWin::GetSystemMetricsInDIP(int metric) {
  return g_instance ? g_instance->GetSystemMetricsForScaleFactor(1.0f, metric)
                    : ::GetSystemMetrics(metric);
}

// static
float ScreenWin::GetScaleFactorForHWND(HWND hwnd) {
  const HWND root_hwnd = g_instance ? g_instance->GetRootWindow(hwnd) : hwnd;
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplayVia(
      &ScreenWin::GetScreenWinDisplayNearestHWND, root_hwnd);
  return screen_win_display.display().device_scale_factor();
}

// static
gfx::Vector2dF ScreenWin::GetPixelsPerInch(const gfx::PointF& point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestDIPPoint,
                             gfx::ToFlooredPoint(point));
  return screen_win_display.pixels_per_inch();
}

// static
int ScreenWin::GetDPIForHWND(HWND hwnd) {
  if (Display::HasForceDeviceScaleFactor())
    return GetDPIFromScalingFactor(Display::GetForcedDeviceScaleFactor());

  const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  return GetPerMonitorDPI(monitor).value_or(
      display::win::internal::GetDefaultSystemDPI());
}

// static
float ScreenWin::GetScaleFactorForDPI(int dpi) {
  return display::win::GetScaleFactorForDPI(dpi, true);
}

// static
float ScreenWin::GetSystemScaleFactor() {
  return display::win::internal::GetUnforcedDeviceScaleFactor();
}

// static
void ScreenWin::SetRequestHDRStatusCallback(
    RequestHDRStatusCallback request_hdr_status_callback) {
  if (g_instance) {
    g_instance->request_hdr_status_callback_ =
        std::move(request_hdr_status_callback);
    g_instance->request_hdr_status_callback_.Run();
  }
}

// static
void ScreenWin::SetHDREnabled(bool hdr_enabled) {
  if (g_instance && (g_instance->hdr_enabled_ != hdr_enabled)) {
    g_instance->hdr_enabled_ = hdr_enabled;
    g_instance->UpdateAllDisplaysAndNotify();
  }
}

HWND ScreenWin::GetHWNDFromNativeView(gfx::NativeView window) const {
  NOTREACHED();
  return nullptr;
}

gfx::NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  NOTREACHED();
  return nullptr;
}

ScreenWin::ScreenWin(bool initialize) {
  DCHECK(!g_instance);
  g_instance = this;
  if (initialize)
    Initialize();
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  ::GetCursorPos(&pt);
  return gfx::ToFlooredPoint(ScreenToDIPPoint(gfx::PointF(gfx::Point(pt))));
}

bool ScreenWin::IsWindowUnderCursor(gfx::NativeWindow window) {
  POINT cursor_loc;
  HWND hwnd =
      ::GetCursorPos(&cursor_loc) ? ::WindowFromPoint(cursor_loc) : nullptr;
  return GetNativeWindowFromHWND(hwnd) == window;
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  const gfx::Point screen_point = DIPToScreenPoint(point);
  return GetNativeWindowFromHWND(WindowFromPoint(screen_point.ToPOINT()));
}

int ScreenWin::GetNumDisplays() const {
  return int{screen_win_displays_.size()};
}

const std::vector<Display>& ScreenWin::GetAllDisplays() const {
  return displays_;
}

Display ScreenWin::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  const HWND window_hwnd = window ? GetHWNDFromNativeView(window) : nullptr;
  // When |window| isn't rooted to a display, we should just return the default
  // display so we get some correct display information like the scaling factor.
  return window_hwnd ? GetScreenWinDisplayNearestHWND(window_hwnd).display()
                     : GetPrimaryDisplay();
}

Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  const gfx::Point screen_point = DIPToScreenPoint(point);
  return GetScreenWinDisplayNearestScreenPoint(screen_point).display();
}

Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return GetScreenWinDisplayNearestScreenRect(match_rect).display();
}

Display ScreenWin::GetPrimaryDisplay() const {
  return GetPrimaryScreenWinDisplay().display();
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

gfx::Rect ScreenWin::ScreenToDIPRectInWindow(
    gfx::NativeView view,
    const gfx::Rect& screen_rect) const {
  const HWND hwnd = view ? GetHWNDFromNativeView(view) : nullptr;
  return ScreenToDIPRect(hwnd, screen_rect);
}

gfx::Rect ScreenWin::DIPToScreenRectInWindow(gfx::NativeView view,
                                             const gfx::Rect& dip_rect) const {
  const HWND hwnd = view ? GetHWNDFromNativeView(view) : nullptr;
  return DIPToScreenRect(hwnd, dip_rect);
}

void ScreenWin::UpdateFromDisplayInfos(
    const std::vector<DisplayInfo>& display_infos) {
  screen_win_displays_ = DisplayInfosToScreenWinDisplays(
      display_infos, color_profile_reader_.get(), hdr_enabled_);
  displays_ = ScreenWinDisplaysToDisplays(screen_win_displays_);
}

void ScreenWin::Initialize() {
  color_profile_reader_->UpdateIfNeeded();
  singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
      base::BindRepeating(&ScreenWin::OnWndProc, base::Unretained(this)));
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  RecordDisplayScaleFactors();

  // We want to remember that we've observed a screen metrics object so that we
  // can remove ourselves as an observer at some later point (either when the
  // metrics object notifies us it's going away or when we are destructed).
  scale_factor_observer_.Add(UwpTextScaleFactor::Instance());
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  return MonitorInfoFromHMONITOR(
      ::MonitorFromPoint(screen_point.ToPOINT(), MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
    const {
  const RECT win_rect = screen_rect.ToRECT();
  return MonitorInfoFromHMONITOR(
      ::MonitorFromRect(&win_rect, MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  return MonitorInfoFromHMONITOR(::MonitorFromWindow(hwnd, default_options));
}

HWND ScreenWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}

int ScreenWin::GetSystemMetrics(int metric) const {
  return ::GetSystemMetrics(metric);
}

void ScreenWin::OnWndProc(HWND hwnd,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message != WM_DISPLAYCHANGE &&
      (message != WM_ACTIVATEAPP || wparam != TRUE) &&
      (message != WM_SETTINGCHANGE || wparam != SPI_SETWORKAREA))
    return;

  color_profile_reader_->UpdateIfNeeded();
  if (request_hdr_status_callback_)
    request_hdr_status_callback_.Run();
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnColorProfilesChanged() {
  // The color profile reader will often just confirm that our guess that the
  // color profile was sRGB was indeed correct. Avoid doing an update in these
  // cases.
  bool changed = false;
  for (const auto& display : displays_) {
    if (display.color_spaces().GetRasterColorSpace() !=
        color_profile_reader_->GetDisplayColorSpace(display.id())) {
      changed = true;
      break;
    }
  }
  if (!changed)
    return;

  UpdateAllDisplaysAndNotify();
}

void ScreenWin::UpdateAllDisplaysAndNotify() {
  std::vector<Display> old_displays = std::move(displays_);
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestHWND(HWND hwnd) const {
  return GetScreenWinDisplay(MonitorInfoFromWindow(hwnd,
                                                   MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPPoint(
    const gfx::Point& dip_point) const {
  ScreenWinDisplay primary_screen_win_display;
  for (const auto& screen_win_display : screen_win_displays_) {
    const gfx::Rect dip_bounds = screen_win_display.display().bounds();
    if (dip_bounds.Contains(dip_point))
      return screen_win_display;
    if (dip_bounds.origin().IsOrigin())
      primary_screen_win_display = screen_win_display;
  }
  return primary_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPRect(
    const gfx::Rect& dip_rect) const {
  ScreenWinDisplay closest_screen_win_display;
  int64_t closest_distance = INT64_MAX;
  for (const auto& screen_win_display : screen_win_displays_) {
    Display display = screen_win_display.display();
    gfx::Rect dip_bounds = display.bounds();
    if (dip_rect.Intersects(dip_bounds))
      return screen_win_display;
    int64_t distance = SquaredDistanceBetweenRects(dip_rect, dip_bounds);
    if (distance < closest_distance) {
      closest_distance = distance;
      closest_screen_win_display = screen_win_display;
    }
  }
  return closest_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetPrimaryScreenWinDisplay() const {
  const ScreenWinDisplay screen_win_display = GetScreenWinDisplay(
      MonitorInfoFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY));
  // The Windows primary monitor is defined to have an origin of (0, 0).
  DCHECK(screen_win_display.display().bounds().origin().IsOrigin());
  return screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplay(
    const MONITORINFOEX& monitor_info) const {
  const int64_t id = DisplayInfo::DeviceIdFromDeviceName(monitor_info.szDevice);
  for (const auto& screen_win_display : screen_win_displays_) {
    if (screen_win_display.display().id() == id)
      return screen_win_display;
  }
  // There is 1:1 correspondence between MONITORINFOEX and ScreenWinDisplay.
  // If we found no screens, either there are no screens, or we're in the midst
  // of updating our screens (see crbug.com/768845); either way, hand out the
  // default display.
  return ScreenWinDisplay();
}

// static
template <typename Getter, typename GetterType>
ScreenWinDisplay ScreenWin::GetScreenWinDisplayVia(Getter getter,
                                                   GetterType value) {
  return g_instance ? (g_instance->*getter)(value) : ScreenWinDisplay();
}

int ScreenWin::GetSystemMetricsForScaleFactor(float scale_factor,
                                              int metric) const {
  if (base::win::IsProcessPerMonitorDpiAware()) {
    static const auto get_system_metrics_for_dpi =
        reinterpret_cast<decltype(&::GetSystemMetricsForDpi)>(
            base::win::GetUser32FunctionPointer("GetSystemMetricsForDpi"));
    if (get_system_metrics_for_dpi) {
      return get_system_metrics_for_dpi(metric,
                                        GetDPIFromScalingFactor(scale_factor));
    }
  }

  // Windows 8.1 doesn't support GetSystemMetricsForDpi(), yet does support
  // per-process dpi awareness.
  return gfx::ToRoundedInt(GetSystemMetrics(metric) * scale_factor /
                           GetPrimaryDisplay().device_scale_factor());
}

void ScreenWin::RecordDisplayScaleFactors() const {
  std::vector<int> unique_scale_factors;
  for (const auto& screen_win_display : screen_win_displays_) {
    const float scale_factor =
        screen_win_display.display().device_scale_factor();
    // Multiply the reported value by 100 to display it as a percentage. Clamp
    // it so that if it's wildly out-of-band we won't send it to the backend.
    const int reported_scale = base::ClampToRange(
        base::checked_cast<int>(scale_factor * 100), 0, 1000);
    if (!base::Contains(unique_scale_factors, reported_scale)) {
      unique_scale_factors.push_back(reported_scale);
      base::UmaHistogramSparse("UI.DeviceScale", reported_scale);
    }
  }
}

void ScreenWin::OnUwpTextScaleFactorChanged() {
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnUwpTextScaleFactorCleanup(UwpTextScaleFactor* source) {
  if (scale_factor_observer_.IsObserving(source))
    scale_factor_observer_.Remove(source);
  UwpTextScaleFactor::Observer::OnUwpTextScaleFactorCleanup(source);
}

}  // namespace win
}  // namespace display
