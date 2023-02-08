#include <algorithm>
#include <ini/extractor.hpp>
#include <ini/flusher.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

namespace ini = gal::ini;

auto main() -> int
{
	std::cout << "Hello GAL INI READER!"
			  << "\nCompiler Name: " << GAL_INI_COMPILER_NAME
			  << "\nCompiler Version: " << GAL_INI_COMPILER_VERSION
			  << "\nINI Version: " << GAL_INI_VERSION
			  << '\n';

	{
		std::cout << "======== CHAR ========\n";

		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> context;
		if (const auto result = ini::extract_from_file("test.ini", context);
			result != ini::ExtractResult::SUCCESS)
		{
			std::cerr << "Error: " << static_cast<int>(result) << '\n';
		}

		std::ranges::for_each(
				context,
				[](const auto& group) -> void
				{
					std::cout << "[" << group.first << "]\n";
					std::ranges::for_each(
							group.second,
							[](const auto& kv) -> void
							{
								std::cout << kv.first << " = " << kv.second << '\n';
							});
				});

		std::cout << "\n";

		if (const auto result = ini::flush_to_file("test_out.ini", context);
			result != ini::FlushResult::SUCCESS)
		{
			std::cerr << "Error: " << static_cast<int>(result) << '\n';
		}
	}
	{
// see ReadMe --> `TODO`
#if !defined(GAL_INI_COMPILER_CLANG)
		std::cout << "======== CHAR8_T ========\n";

		std::unordered_map<std::u8string, std::unordered_map<std::u8string, std::u8string>> context;
		if (const auto result = ini::extract_from_file("test.ini", context);
			result != ini::ExtractResult::SUCCESS)
		{
			std::cerr << "Error: " << static_cast<int>(result) << '\n';
		}

		std::ranges::for_each(
				context,
				[](const auto& group) -> void
				{
					std::cout << "[" << reinterpret_cast<const std::string&>(group.first) << "]\n";
					std::ranges::for_each(
							group.second,
							[](const auto& kv) -> void
							{
								std::cout << reinterpret_cast<const std::string&>(kv.first) << " = " << reinterpret_cast<const std::string&>(kv.second) << '\n';
							});
				});

		std::cout << "\n";
	}
#endif
}
