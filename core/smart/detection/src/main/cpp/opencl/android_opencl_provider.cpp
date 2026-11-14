/*
 * Copyright (C) 2026 Smart AutoClicker contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "android_opencl_provider.hpp"

#include <android/log.h>
#include <dlfcn.h>

#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr char kLogTag[] = "SmartAutoClickerOpenCL";
constexpr char kDirectProviderEnv[] = "SMART_AUTOCLICKER_OPENCL_DIRECT_LIBRARY";
constexpr char kIcdFilenamesEnv[] = "OCL_ICD_FILENAMES";

bool contains(const std::vector<std::string>& values, const std::string& value) {
    for (const auto& current : values) {
        if (current == value) return true;
    }
    return false;
}

void addUnique(std::vector<std::string>& values, const char* value) {
    if (!value || !*value) return;
    const std::string candidate(value);
    if (!contains(values, candidate)) values.push_back(candidate);
}

/**
 * Symbol-only check: some OEM drivers can block inside clGetPlatformIDs().
 * Provider discovery intentionally never calls into the OpenCL implementation.
 */
bool inspectProvider(const std::string& path, bool* isIcd) {
    if (isIcd) *isIcd = false;

    dlerror();
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) return false;

    dlerror();
    void* platformIds = dlsym(handle, "clGetPlatformIDs");
    const char* platformError = dlerror();

    dlerror();
    void* icdPlatformIds = dlsym(handle, "clIcdGetPlatformIDsKHR");
    const char* icdError = dlerror();

    const bool hasPlatformIds = platformIds != nullptr && platformError == nullptr;
    if (isIcd) *isIcd = icdPlatformIds != nullptr && icdError == nullptr;

    dlclose(handle);
    return hasPlatformIds;
}

std::vector<std::string> directProviderCandidates() {
    const bool is64Bit = sizeof(void*) == 8;
    std::vector<std::string> candidates;

    // Prefer concrete vendor paths before generic SONAMEs.
    if (is64Bit) {
        addUnique(candidates, "/system/vendor/lib64/libOpenCL.so");
        addUnique(candidates, "/vendor/lib64/libOpenCL.so");
        addUnique(candidates, "/odm/lib64/libOpenCL.so");
        addUnique(candidates, "/product/lib64/libOpenCL.so");
        addUnique(candidates, "/system_ext/vendor/lib64/libOpenCL.so");

        addUnique(candidates, "/system/vendor/lib64/egl/libGLES_mali.so");
        addUnique(candidates, "/vendor/lib64/egl/libGLES_mali.so");
        addUnique(candidates, "/odm/lib64/egl/libGLES_mali.so");
        addUnique(candidates, "/system/vendor/lib64/libGLES_mali.so");
        addUnique(candidates, "/vendor/lib64/libGLES_mali.so");
        addUnique(candidates, "/odm/lib64/libGLES_mali.so");

        addUnique(candidates, "/system/vendor/lib64/libmali.so");
        addUnique(candidates, "/vendor/lib64/libmali.so");
        addUnique(candidates, "/odm/lib64/libmali.so");
        addUnique(candidates, "/system/vendor/lib64/libPVROCL.so");
        addUnique(candidates, "/vendor/lib64/libPVROCL.so");
        addUnique(candidates, "/odm/lib64/libPVROCL.so");
    } else {
        addUnique(candidates, "/system/vendor/lib/libOpenCL.so");
        addUnique(candidates, "/vendor/lib/libOpenCL.so");
        addUnique(candidates, "/odm/lib/libOpenCL.so");
        addUnique(candidates, "/product/lib/libOpenCL.so");
        addUnique(candidates, "/system_ext/vendor/lib/libOpenCL.so");

        addUnique(candidates, "/system/vendor/lib/egl/libGLES_mali.so");
        addUnique(candidates, "/vendor/lib/egl/libGLES_mali.so");
        addUnique(candidates, "/odm/lib/egl/libGLES_mali.so");
        addUnique(candidates, "/system/vendor/lib/libGLES_mali.so");
        addUnique(candidates, "/vendor/lib/libGLES_mali.so");
        addUnique(candidates, "/odm/lib/libGLES_mali.so");

        addUnique(candidates, "/system/vendor/lib/libmali.so");
        addUnique(candidates, "/vendor/lib/libmali.so");
        addUnique(candidates, "/odm/lib/libmali.so");
        addUnique(candidates, "/system/vendor/lib/libPVROCL.so");
        addUnique(candidates, "/vendor/lib/libPVROCL.so");
        addUnique(candidates, "/odm/lib/libPVROCL.so");
    }

    // Some Android builds expose only linker-resolved vendor SONAMEs.
    addUnique(candidates, "libOpenCL.so");
    addUnique(candidates, "libOpenCL.so.1");
    addUnique(candidates, "libGLES_mali.so");
    addUnique(candidates, "libmali.so");
    addUnique(candidates, "libPVROCL.so");
    addUnique(candidates, "libOpenCL-pixel.so");
    addUnique(candidates, "libOpenCL-car.so");

    return candidates;
}

std::string configureOnce() {
    const char* existingDirect = std::getenv(kDirectProviderEnv);
    if (existingDirect && *existingDirect) {
        return std::string("Preserving existing Android direct OpenCL provider: ") + existingDirect;
    }

    const char* existingIcds = std::getenv(kIcdFilenamesEnv);
    if (existingIcds && *existingIcds) {
        return std::string("Preserving existing OCL_ICD_FILENAMES: ") + existingIcds;
    }

    for (const auto& path : directProviderCandidates()) {
        bool isIcd = false;
        if (!inspectProvider(path, &isIcd)) continue;

        // A Khronos ICD/loader remains on the standard ICD path. Never pass a
        // loader SONAME to itself as a direct vendor provider.
        if (isIcd) continue;

        if (setenv(kDirectProviderEnv, path.c_str(), 1) == 0) {
            return std::string("Selected Android direct OpenCL provider: ") + path +
                   " (symbol-only discovery; no OpenCL driver calls made)";
        }

        return std::string("Found Android OpenCL provider at ") + path +
               " but setenv(" + kDirectProviderEnv + ") failed";
    }

    return "No Android direct OpenCL provider selected; standard Khronos ICD discovery remains enabled";
}

} // namespace

namespace smartautoclicker {

std::string configureAndroidOpenCLProvider() {
    static std::once_flag once;
    static std::string report;

    std::call_once(once, [] {
        report = configureOnce();
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s", report.c_str());
    });

    return report;
}

} // namespace smartautoclicker
