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
	const gal::ini::IniParser ini{"test.ini"};
	ini.print(std::cout);

	std::cout << "\n\n";

	std::cout << "=== unordered parser with comment ===\n";
	const gal::ini::IniParserWithComment ini_with_comment{"test.ini"};
	ini_with_comment.print(std::cout);
}
