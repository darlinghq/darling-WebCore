/*
 * Copyright (C) 2011, 2014 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryRelease.h"

#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "Document.h"
#include "FontCache.h"
#include "GCController.h"
#include "HTMLMediaElement.h"
#include "InlineStyleSheetOwner.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageCache.h"
#include "ScrollingThread.h"
#include "StyleScope.h"
#include "StyledElement.h"
#include "WorkerThread.h"
#include <wtf/FastMalloc.h>

#if PLATFORM(COCOA)
#include "ResourceUsageThread.h"
#endif

namespace WebCore {

static void releaseNoncriticalMemory()
{
    FontCache::singleton().purgeInactiveFontData();

    clearWidthCaches();

    for (auto* document : Document::allDocuments())
        document->clearSelectorQueryCache();

    MemoryCache::singleton().pruneDeadResourcesToSize(0);

    StyledElement::clearPresentationAttributeCache();

    InlineStyleSheetOwner::clearCache();
}

static void releaseCriticalMemory(Synchronous synchronous)
{
    // Right now, the only reason we call release critical memory while not under memory pressure is if the process is about to be suspended.
    PruningReason pruningReason = MemoryPressureHandler::singleton().isUnderMemoryPressure() ? PruningReason::MemoryPressure : PruningReason::ProcessSuspended;
    PageCache::singleton().pruneToSizeNow(0, pruningReason);

    MemoryCache::singleton().pruneLiveResourcesToSize(0, /*shouldDestroyDecodedDataForAllLiveResources*/ true);

    CSSValuePool::singleton().drain();

    Vector<RefPtr<Document>> documents;
    copyToVector(Document::allDocuments(), documents);
    for (auto& document : documents)
        document->styleScope().clearResolver();

    GCController::singleton().deleteAllCode(JSC::DeleteAllCodeIfNotCollecting);

#if ENABLE(VIDEO)
    for (auto* mediaElement : HTMLMediaElement::allMediaElements()) {
        if (mediaElement->paused())
            mediaElement->purgeBufferedDataIfPossible();
    }
#endif

    if (synchronous == Synchronous::Yes) {
        GCController::singleton().garbageCollectNow();
    } else {
#if PLATFORM(IOS)
        GCController::singleton().garbageCollectNowIfNotDoneRecently();
#else
        GCController::singleton().garbageCollectSoon();
#endif
    }

    // We reduce tiling coverage while under memory pressure, so make sure to drop excess tiles ASAP.
    Page::forEachPage([](Page& page) {
        page.chrome().client().scheduleCompositingLayerFlush();
    });
}

void releaseMemory(Critical critical, Synchronous synchronous)
{
    if (critical == Critical::Yes)
        releaseCriticalMemory(synchronous);

    releaseNoncriticalMemory();

    platformReleaseMemory(critical);

    // FastMalloc has lock-free thread specific caches that can only be cleared from the thread itself.
    WorkerThread::releaseFastMallocFreeMemoryInAllThreads();
#if ENABLE(ASYNC_SCROLLING) && !PLATFORM(IOS)
    ScrollingThread::dispatch([]() {
        WTF::releaseFastMallocFreeMemory();
    });
#endif
    WTF::releaseFastMallocFreeMemory();

#if ENABLE(RESOURCE_USAGE)
    Page::forEachPage([&](Page& page) {
        InspectorInstrumentation::didHandleMemoryPressure(page, critical);
    });
#endif
}

#if !RELEASE_LOG_DISABLED
static unsigned pageCount()
{
    unsigned count = 0;
    Page::forEachPage([&] (Page& page) {
        if (!page.isUtilityPage())
            ++count;
    });
    return count;
}
#endif

void logMemoryStatisticsAtTimeOfDeath()
{
#if !RELEASE_LOG_DISABLED
#if PLATFORM(COCOA)
    auto pageSize = vmPageSize();
    auto pages = pagesPerVMTag();

    RELEASE_LOG(MemoryPressure, "Dirty memory per VM tag at time of death:");
    for (unsigned i = 0; i < 256; ++i) {
        size_t dirty = pages[i].dirty * pageSize;
        if (!dirty)
            continue;
        String tagName = displayNameForVMTag(i);
        if (!tagName)
            tagName = String::format("Tag %u", i);
        RELEASE_LOG(MemoryPressure, "%16s: %lu MB", tagName.latin1().data(), dirty / MB);
    }
#endif

    auto& vm = commonVM();
    RELEASE_LOG(MemoryPressure, "Memory usage statistics at time of death:");
    RELEASE_LOG(MemoryPressure, "GC heap size: %zu", vm.heap.size());
    RELEASE_LOG(MemoryPressure, "GC heap extra memory size: %zu", vm.heap.extraMemorySize());
#if ENABLE(RESOURCE_USAGE)
    RELEASE_LOG(MemoryPressure, "GC heap external memory: %zu", vm.heap.externalMemorySize());
#endif
    RELEASE_LOG(MemoryPressure, "Global object count: %zu", vm.heap.globalObjectCount());

    RELEASE_LOG(MemoryPressure, "Page count: %u", pageCount());
    RELEASE_LOG(MemoryPressure, "Document count: %u", Document::allDocuments().size());
    RELEASE_LOG(MemoryPressure, "Live JavaScript objects:");
    for (auto& it : *vm.heap.objectTypeCounts())
        RELEASE_LOG(MemoryPressure, "  %s: %d", it.key, it.value);
#endif
}

void didExceedMemoryLimitAndFailedToRecover()
{
    RELEASE_LOG(MemoryPressure, "Crashing non-visible process due to excessive memory usage + inability to free up memory below panic threshold.");
    logMemoryStatisticsAtTimeOfDeath();
    CRASH();
}

bool processIsEligibleForMemoryKill()
{
    bool hasVisiblePages = false;
    bool hasAudiblePages = false;
    bool hasMainFrameNavigatedInTheLastHour = false;

    auto now = MonotonicTime::now();
    Page::forEachPage([&] (Page& page) {
        if (page.isUtilityPage())
            return;
        if (page.isVisible())
            hasVisiblePages = true;
        if (page.activityState() & ActivityState::IsAudible)
            hasAudiblePages = true;
        if (auto timeOfLastCompletedLoad = page.mainFrame().timeOfLastCompletedLoad()) {
            if (now - timeOfLastCompletedLoad <= Seconds::fromMinutes(60))
                hasMainFrameNavigatedInTheLastHour = true;
        }
    });

    bool eligible = !hasVisiblePages && !hasAudiblePages && !hasMainFrameNavigatedInTheLastHour;
    if (!eligible)
        RELEASE_LOG(MemoryPressure, "Process not eligible for panic memory kill. Reasons: hasVisiblePages=%u, hasAudiblePages=%u, hasMainFrameNavigatedInTheLastHour=%u", hasVisiblePages, hasAudiblePages, hasMainFrameNavigatedInTheLastHour);

    return eligible;
}

#if !PLATFORM(COCOA)
void platformReleaseMemory(Critical) { }
void jettisonExpensiveObjectsOnTopLevelNavigation() { }
void registerMemoryReleaseNotifyCallbacks() { }
#endif

} // namespace WebCore
