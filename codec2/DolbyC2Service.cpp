/*
 * SPDX-FileCopyrightText: acerhizm
 * SPDX-License-Identifier: Apache-2.0
 */

// AIDL Codec2 service around a Dolby C2ComponentStore blob, after
// frameworks/av/media/codec2/hal/services/vendor.cpp. Codec2Client lists either
// the AIDL or the HIDL stores, never both, and the 6.6 QTI store is AIDL only
// (media.c2.hal.selection=aidl), so the HIDL Dolby services would stay unused.

#define LOG_TAG "dolby-c2-service"

#include <signal.h>

#include <thread>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <codec2/aidl/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>

namespace android {
// Exported by libcodec2_store_dolby.so (audio) and c2.dolby.store.so (Dolby Vision).
std::shared_ptr<C2ComponentStore> GetCodec2DolbyComponentStore();
}  // namespace android

static constexpr int kThreadCount = 8;

int main() {
    using namespace ::aidl::android::hardware::media::c2;

    signal(SIGPIPE, SIG_IGN);
    LOG(INFO) << "Dolby Codec2 AIDL service (" << DOLBY_C2_INSTANCE << ") starting...";

    // As the HIDL services did: vndbinder for vendor services, and a hwbinder pool,
    // since the components are HIDL clients (DMS, the QTI store of the Dolby Vision
    // client) and get their callbacks on hwbinder threads.
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();
    std::thread([] {
        android::hardware::configureRpcThreadpool(kThreadCount, true /* callerWillJoin */);
        android::hardware::joinRpcThreadpool();
    }).detach();

    ABinderProcess_setThreadPoolMaxThreadCount(kThreadCount);
    ABinderProcess_startThreadPool();

    std::shared_ptr<C2ComponentStore> c2Store = android::GetCodec2DolbyComponentStore();
    CHECK(c2Store) << "Cannot create the Dolby C2ComponentStore.";

    std::shared_ptr<IComponentStore> store =
            ::ndk::SharedRefBase::make<utils::ComponentStore>(c2Store);
    const std::string instance =
            std::string(IComponentStore::descriptor) + "/" + DOLBY_C2_INSTANCE;
    binder_exception_t ex = AServiceManager_addService(store->asBinder().get(), instance.c_str());
    CHECK_EQ(ex, EX_NONE) << "Cannot register " << instance;
    LOG(INFO) << "Registered " << instance;

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
