/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AVCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVAudioCaptureSource.h"
#import "AVMediaCaptureSource.h"
#import "AVVideoCaptureSource.h"
#import "AudioSourceProvider.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSource.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeMediaSourceSupportedConstraints.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureSession AVCaptureSessionType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureSession)

#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureSession getAVCaptureSessionClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeMuxed, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPresetLow, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureDeviceWasConnectedNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureDeviceWasDisconnectedNotification, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMuxed getAVMediaTypeMuxed()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()
#define AVCaptureDeviceWasConnectedNotification getAVCaptureDeviceWasConnectedNotification()
#define AVCaptureDeviceWasDisconnectedNotification getAVCaptureDeviceWasDisconnectedNotification()

using namespace WebCore;

@interface WebCoreAVCaptureDeviceManagerObserver : NSObject
{
    AVCaptureDeviceManager* m_callback;
}

-(id)initWithCallback:(AVCaptureDeviceManager*)callback;
-(void)disconnect;
-(void)deviceDisconnected:(NSNotification *)notification;
-(void)deviceConnected:(NSNotification *)notification;
@end

namespace WebCore {

AVCaptureSessionInfo::AVCaptureSessionInfo(AVCaptureSessionType *platformSession)
    : m_platformSession(platformSession)
{
}

bool AVCaptureSessionInfo::supportsVideoSize(const String& videoSize) const
{
    return [m_platformSession canSetSessionPreset:videoSize];
}

String AVCaptureSessionInfo::bestSessionPresetForVideoDimensions(int width, int height) const
{
    ASSERT(width >= 0);
    ASSERT(height >= 0);

    if (width > 1280 || height > 720) {
        // FIXME: this restriction could be adjusted with the videoMaxScaleAndCropFactor property.
        return emptyString();
    }

    if (width > 640 || height > 480) {
        if (supportsVideoSize(AVCaptureSessionPreset1280x720))
            return AVCaptureSessionPreset1280x720;

        return emptyString();
    }

    if (width > 352 || height > 288) {
        if (supportsVideoSize(AVCaptureSessionPreset640x480))
            return AVCaptureSessionPreset640x480;

        return emptyString();
    }

    if (supportsVideoSize(AVCaptureSessionPreset352x288))
        return AVCaptureSessionPreset352x288;

    if (supportsVideoSize(AVCaptureSessionPresetLow))
        return AVCaptureSessionPresetLow;

    return emptyString();
}


Vector<CaptureDeviceInfo>& AVCaptureDeviceManager::captureDeviceList()
{
    if (!isAvailable())
        return m_devices;

    static bool firstTime = true;
    if (firstTime && !m_devices.size()) {
        firstTime = false;
        refreshCaptureDeviceList();
        registerForDeviceNotifications();
    }

    return m_devices;
}

inline static bool shouldConsiderDeviceInDeviceList(AVCaptureDeviceTypedef *device)
{
    if (![device isConnected])
        return false;

#if !PLATFORM(IOS)
    if ([device isSuspended] || [device isInUseByAnotherApplication])
        return false;
#endif

    return true;
}

void AVCaptureDeviceManager::refreshCaptureDeviceList()
{
    for (AVCaptureDeviceTypedef *platformDevice in [getAVCaptureDeviceClass() devices]) {
        if (!shouldConsiderDeviceInDeviceList(platformDevice))
            continue;

        CaptureDeviceInfo captureDevice;
        if (!captureDeviceFromDeviceID(platformDevice.uniqueID, captureDevice)) {
            // An AVCaptureDevice has a unique ID, but we can't use it for the source ID because:
            // 1. if it provides both audio and video we will need to create two sources for it
            // 2. the unique ID persists on one system across device connections, disconnections,
            //    application restarts, and reboots, so it could be used to figerprint a user.
            captureDevice.m_persistentDeviceID = platformDevice.uniqueID;
            captureDevice.m_enabled = true;
            captureDevice.m_localizedName = platformDevice.localizedName;
            if ([platformDevice position] == AVCaptureDevicePositionFront)
                captureDevice.m_position = RealtimeMediaSourceSettings::User;
            if ([platformDevice position] == AVCaptureDevicePositionBack)
                captureDevice.m_position = RealtimeMediaSourceSettings::Environment;

            bool hasAudio = [platformDevice hasMediaType:AVMediaTypeAudio] || [platformDevice hasMediaType:AVMediaTypeMuxed];
            bool hasVideo = [platformDevice hasMediaType:AVMediaTypeVideo] || [platformDevice hasMediaType:AVMediaTypeMuxed];
            if (!hasAudio && !hasVideo)
                continue;

            // FIXME: For a given device, the source ID should persist when visiting the same request origin,
            // but differ across different request origins.
            captureDevice.m_sourceId = createCanonicalUUIDString();
            captureDevice.m_sourceType = hasVideo ? RealtimeMediaSource::Video : RealtimeMediaSource::Audio;
            if (hasVideo && hasAudio) {
                // Add the audio component as a separate device.
                CaptureDeviceInfo audioCaptureDevice = captureDevice;
                audioCaptureDevice.m_sourceId = createCanonicalUUIDString();
                audioCaptureDevice.m_sourceType = RealtimeMediaSource::Audio;
                m_devices.append(audioCaptureDevice);
            }
            m_devices.append(captureDevice);
        }
    }
}

bool AVCaptureDeviceManager::isAvailable()
{
    return AVFoundationLibrary();
}

AVCaptureDeviceManager& AVCaptureDeviceManager::singleton()
{
    static NeverDestroyed<AVCaptureDeviceManager> manager;
    return manager;
}

AVCaptureDeviceManager::AVCaptureDeviceManager()
    : m_objcObserver(adoptNS([[WebCoreAVCaptureDeviceManagerObserver alloc] initWithCallback: this]))
{
}

AVCaptureDeviceManager::~AVCaptureDeviceManager()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];
}

Vector<CaptureDevice> AVCaptureDeviceManager::getSourcesInfo()
{
    if (!isAvailable())
        return Vector<CaptureDevice>();

    return CaptureDeviceManager::getSourcesInfo();
}

RefPtr<RealtimeMediaSource> AVCaptureDeviceManager::createMediaSourceForCaptureDeviceWithConstraints(const CaptureDeviceInfo& captureDevice, const MediaConstraints* constraints, String& invalidConstraint)
{
    AVCaptureDeviceTypedef *device = [getAVCaptureDeviceClass() deviceWithUniqueID:captureDevice.m_persistentDeviceID];
    if (!device)
        return nullptr;

    if (captureDevice.m_sourceType == RealtimeMediaSource::Audio)
        return AVAudioCaptureSource::create(device, captureDevice.m_sourceId, constraints, invalidConstraint);

    return AVVideoCaptureSource::create(device, captureDevice.m_sourceId, constraints, invalidConstraint);
}

void AVCaptureDeviceManager::registerForDeviceNotifications()
{
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnected:) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceDisconnected:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
}

void AVCaptureDeviceManager::deviceConnected()
{
    refreshCaptureDeviceList();
}

void AVCaptureDeviceManager::deviceDisconnected(AVCaptureDeviceTypedef* device)
{
    Vector<CaptureDeviceInfo>& devices = captureDeviceList();

    size_t count = devices.size();
    if (!count)
        return;

    String deviceID = device.uniqueID;
    for (size_t i = 0; i < count; ++i) {
        if (devices[i].m_persistentDeviceID == deviceID) {
            LOG(Media, "AVCaptureDeviceManager::deviceDisconnected(%p), device %d disabled", this, i);
            devices[i].m_enabled = false;
        }
    }
}

} // namespace WebCore

@implementation WebCoreAVCaptureDeviceManagerObserver

- (id)initWithCallback:(AVCaptureDeviceManager*)callback
{
    self = [super init];
    if (!self)
        return nil;
    m_callback = callback;
    return self;
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    m_callback = 0;
}

- (void)deviceDisconnected:(NSNotification *)notification
{
    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback) {
            AVCaptureDeviceTypedef *device = [notification object];
            m_callback->deviceDisconnected(device);
        }
    });
}

- (void)deviceConnected:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback)
            m_callback->deviceConnected();
    });
}

@end

#endif // ENABLE(MEDIA_STREAM)
