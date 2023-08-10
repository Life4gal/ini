#include <map>
#include <string>

#include "test_extractor_generate_file.hpp"

using namespace gal::ini;
using namespace boost::ut;

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
#define GAL_INI_NO_DESTROY [[clang::no_destroy]]
#else
#define GAL_INI_NO_DESTROY
#endif

namespace
{
	class UserString
	{
	public:
		using string_type = std::string;
		using string_view_type = std::string_view;

		using value_type = string_type::value_type;

	private:
		string_type string_;

	public:
		explicit(false) UserString(string_type&& string)
			: string_{std::move(string)} {}

		explicit(false) UserString(const char* string)
			: string_{string} {}

		// for appender_traits::allocatable
		[[maybe_unused]] auto push_back(const value_type c) -> void { string_.push_back(c); }

		[[nodiscard]] auto data() const noexcept -> const string_type& { return string_; }

		[[maybe_unused]] friend auto operator<=>(const UserString& lhs, const UserString& rhs) noexcept -> auto { return lhs.data() <=> rhs.data(); }

		[[maybe_unused]] friend auto operator<=>(const UserString& lhs, const string_type& rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		[[maybe_unused]] friend auto operator<=>(const string_type& lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		[[maybe_unused]] friend auto operator<=>(const UserString& lhs, const string_view_type rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		[[maybe_unused]] friend auto operator<=>(const string_view_type lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		[[maybe_unused]] friend auto operator<=>(const UserString& lhs, const string_type::pointer rhs) noexcept -> auto { return lhs.data() <=> rhs; }

		[[maybe_unused]] friend auto operator<=>(const string_type::pointer lhs, const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		template<std::size_t N>
		[[maybe_unused]] friend auto operator<=>(const UserString& lhs, const string_type::value_type (&rhs)[N]) noexcept -> auto { return lhs.data() <=> rhs; }

		template<std::size_t N>
		[[maybe_unused]] friend auto operator<=>(const string_type::value_type (&lhs)[N], const UserString& rhs) noexcept -> auto { return lhs <=> rhs.data(); }

		[[nodiscard]] explicit(false) operator string_view_type() const noexcept { return string_; }
	};

	// UserString::push_back
	static_assert(appender_traits<UserString>::allocatable);

	using group_type = std::map<UserString, UserString, std::less<>>;
	using context_type = std::map<UserString, group_type, std::less<>>;

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_generate_file = [] { do_generate_file(); };

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_file = [] { do_test_extract_from_file<context_type>(); };

	GAL_INI_NO_DESTROY [[maybe_unused]] suite suite_extract_from_buffer = [] { do_test_extract_from_buffer<context_type>(); };
}
