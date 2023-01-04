#include <ini/ini.hpp>
#include <iostream>

auto main() -> int
{
	std::cout << "Hello GAL INI READER!"
			  << "\nCompiler Name: " << GAL_INI_COMPILER_NAME
			  << "\nCompiler Version: " << GAL_INI_COMPILER_VERSION
			  << "\nINI Version: " << GAL_INI_VERSION
			  << '\n';

	std::cout << "=== unordered parser ===\n";
	{
		const auto [result, data] = gal::ini::IniExtractor::extract_from_file("test.ini");
		if (result != gal::ini::impl::FileExtractResult::SUCCESS)
		{
			std::cout << "Error: " << static_cast<int>(result);
		}
		gal::ini::IniFlusher flusher{data};
		flusher.flush(std::cout);
	}

	std::cout << "\n\n";

	std::cout << "=== unordered parser with comment ===\n";
	{
		const auto [result, data] = gal::ini::IniExtractorWithComment::extract_from_file("test.ini");
		if (result != gal::ini::impl::FileExtractResult::SUCCESS)
		{
			std::cout << "Error: " << static_cast<int>(result);
		}
		gal::ini::IniFlusherWithComment flusher{data};
		flusher.flush(std::cout);
	}
}
