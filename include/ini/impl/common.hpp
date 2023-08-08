// Copyright (C) 2022-2023 Life4gal <life4gal@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#pragma once

#include <string>

#if defined(GAL_INI_SHARED_LIBRARY)
#if defined(GAL_INI_PLATFORM_WINDOWS)
		#define GAL_INI_SYMBOL_EXPORT __declspec(dllexport)
#else
		#define GAL_INI_SYMBOL_EXPORT __attribute__((visibility("default")))
#endif
#else
#define GAL_INI_SYMBOL_EXPORT
#endif

namespace gal::ini
{
	using string_type = std::string;
	using string_view_type = std::string_view;
}
