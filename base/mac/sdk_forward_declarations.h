// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains forward declarations for items in later SDKs than the
// default one with which Chromium is built (currently 10.10).
// If you call any function from this header, be sure to check at runtime for
// respondsToSelector: before calling these functions (else your code will crash
// on older OS X versions that chrome still supports).

#ifndef BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
#define BASE_MAC_SDK_FORWARD_DECLARATIONS_H_

#import <AppKit/AppKit.h>
#include <AvailabilityMacros.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreWLAN/CoreWLAN.h>
#import <IOBluetooth/IOBluetooth.h>
#import <ImageCaptureCore/ImageCaptureCore.h>
#import <QuartzCore/QuartzCore.h>
#include <stdint.h>

#include "base/base_export.h"

// ----------------------------------------------------------------------------
// Define typedefs, enums, and protocols not available in the version of the
// OSX SDK being compiled against.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Define NSStrings only available in newer versions of the OSX SDK to force
// them to be statically linked.
// ----------------------------------------------------------------------------

extern "C" {
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_11
BASE_EXPORT extern NSString* const CIDetectorTypeText;
#endif  // MAC_OS_X_VERSION_10_11
}  // extern "C"

// Once Chrome no longer supports OSX 10.12, everything within this
// preprocessor block can be removed.
#if !defined(MAC_OS_X_VERSION_10_13) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_13

// VNRequest forward declarations.
@class VNRequest;
typedef void (^VNRequestCompletionHandler)(VNRequest* request, NSError* error);

@interface VNRequest : NSObject<NSCopying>
- (instancetype)initWithCompletionHandler:
    (VNRequestCompletionHandler)completionHandler NS_DESIGNATED_INITIALIZER;
@property(readonly, nonatomic, copy) NSArray* results;
@end

// VNDetectFaceLandmarksRequest forward declarations.
@interface VNImageBasedRequest : VNRequest
@end

@protocol VNFaceObservationAccepting<NSObject>
@end

@interface VNDetectFaceLandmarksRequest
    : VNImageBasedRequest<VNFaceObservationAccepting>
@end

// VNImageRequestHandler forward declarations.
typedef NSString* VNImageOption NS_STRING_ENUM;

@interface VNImageRequestHandler : NSObject
- (instancetype)initWithCIImage:(CIImage*)image
                        options:(NSDictionary<VNImageOption, id>*)options;
- (BOOL)performRequests:(NSArray<VNRequest*>*)requests error:(NSError**)error;
@end

// VNFaceLandmarks2D forward declarations.
@interface VNFaceLandmarkRegion : NSObject
@property(readonly) NSUInteger pointCount;
@end

@interface VNFaceLandmarkRegion2D : VNFaceLandmarkRegion
@property(readonly, assign)
    const CGPoint* normalizedPoints NS_RETURNS_INNER_POINTER;
@end

@interface VNFaceLandmarks2D : NSObject
@property(readonly) VNFaceLandmarkRegion2D* leftEye;
@property(readonly) VNFaceLandmarkRegion2D* rightEye;
@property(readonly) VNFaceLandmarkRegion2D* outerLips;
@property(readonly) VNFaceLandmarkRegion2D* nose;
@end

// VNFaceObservation forward declarations.
@interface VNObservation : NSObject<NSCopying, NSSecureCoding>
@end

@interface VNDetectedObjectObservation : VNObservation
@property(readonly, nonatomic, assign) CGRect boundingBox;
@end

@interface VNFaceObservation : VNDetectedObjectObservation
@property(readonly, nonatomic, strong) VNFaceLandmarks2D* landmarks;
@end

// VNDetectBarcodesRequest forward declarations.
typedef NSString* VNBarcodeSymbology NS_STRING_ENUM;

@interface VNDetectBarcodesRequest : VNImageBasedRequest
@property(readwrite, nonatomic, copy) NSArray<VNBarcodeSymbology>* symbologies;
@end

// VNBarcodeObservation forward declarations.
@interface VNRectangleObservation : VNDetectedObjectObservation
@property(readonly, nonatomic, assign) CGPoint topLeft;
@property(readonly, nonatomic, assign) CGPoint topRight;
@property(readonly, nonatomic, assign) CGPoint bottomLeft;
@property(readonly, nonatomic, assign) CGPoint bottomRight;
@end

@interface VNBarcodeObservation : VNRectangleObservation
@property(readonly, nonatomic, copy) NSString* payloadStringValue;
@property(readonly, nonatomic, copy) VNBarcodeSymbology symbology;
@end

#endif  // MAC_OS_X_VERSION_10_13

#if !defined(MAC_OS_X_VERSION_10_15) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_15

@interface NSScreen (ForwardDeclare)
@property(readonly)
    CGFloat maximumPotentialExtendedDynamicRangeColorComponentValue;
@end

@interface SFUniversalLink : NSObject
- (instancetype)initWithWebpageURL:(NSURL*)url;
@property(readonly) NSURL* webpageURL;
@property(readonly) NSURL* applicationURL;
@property(getter=isEnabled) BOOL enabled;
@end

#endif  // MAC_OS_X_VERSION_10_15

// ----------------------------------------------------------------------------
// The symbol for kCWSSIDDidChangeNotification is available in the
// CoreWLAN.framework for OSX versions 10.6 through 10.10. The symbol is not
// declared in the OSX 10.9+ SDK, so when compiling against an OSX 10.9+ SDK,
// declare the symbol.
// ----------------------------------------------------------------------------
BASE_EXPORT extern "C" NSString* const kCWSSIDDidChangeNotification;

#endif  // BASE_MAC_SDK_FORWARD_DECLARATIONS_H_
