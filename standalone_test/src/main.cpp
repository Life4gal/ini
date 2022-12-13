#include <ini/ini.hpp>
#include <iostream>

auto main() -> int
{
	std::cout << "Hello GAL INI READER!"
			  << "\nCompiler Name: " << GAL_INI_COMPILER_NAME
			  << "\nCompiler Version: " << GAL_INI_COMPILER_VERSION
			  << "\nINI Version: " << GAL_INI_VERSION
			  << '\n';

	const gal::ini::Ini ini{"test.ini"};
	ini.print(std::cout);
}
