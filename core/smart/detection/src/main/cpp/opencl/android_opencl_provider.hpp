/*
 * Copyright (C) 2026 Smart AutoClicker contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef SMART_AUTOCLICKER_ANDROID_OPENCL_PROVIDER_HPP
#define SMART_AUTOCLICKER_ANDROID_OPENCL_PROVIDER_HPP

#include <string>

namespace smartautoclicker {

/**
 * Selects a device-supplied Android OpenCL provider without calling any OpenCL
 * entry point. The Khronos loader performs the actual provider dispatch.
 */
std::string configureAndroidOpenCLProvider();

} // namespace smartautoclicker

#endif // SMART_AUTOCLICKER_ANDROID_OPENCL_PROVIDER_HPP
