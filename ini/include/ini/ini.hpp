#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <filesystem>

#ifndef GAL_INI_UNORDERED_MAP_TYPE
#include <unordered_map>
#endif

#if defined(GAL_INI_COMPILER_MSVC) || defined(_MSC_VER)
#define GAL_INI_UNREACHABLE() __assume(0)
#elif defined(GAL_INI_COMPILER_GNU) || defined(GAL_INI_COMPILER_CLANG) || defined(__GNUC__) || defined(__clang__)
	#define GAL_INI_UNREACHABLE() __builtin_unreachable()
#else
	#define GAL_INI_UNREACHABLE() throw
#endif

namespace gal::ini
{
	#ifndef GAL_INI_STRING_TYPE
	using string_type = std::string;
	#else
	using string_type = GAL_INI_STRING_TYPE;
	#endif

	using char_type = string_type::value_type;
	using string_view_type = std::basic_string_view<char_type>;

	using file_path_type = std::filesystem::path;

	struct comment_view_type
	{
		char_type        indication;
		string_view_type comment;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool { return comment.empty(); }
	};

	struct comment_type
	{
		char_type   indication;
		string_type comment;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool { return comment.empty(); }

		[[nodiscard]] constexpr explicit(false) operator comment_view_type() const noexcept { return {indication, comment}; }
	};

	#ifndef GAL_INI_STRING_HASH_TYPE
	struct string_hash_type
	{
		using is_transparent = int;

		[[nodiscard]] auto operator()(const string_type& string) const noexcept -> std::size_t { return std::hash<string_type>{}(string); }

		[[nodiscard]] auto operator()(const string_view_type& string) const noexcept -> std::size_t { return std::hash<string_view_type>{}(string); }
	};
	#else
	using string_hash_type = GAL_INI_STRING_HASH_TYPE
	#endif

	template<typename Key, typename Value, typename KeyHash, typename KeyComparator = std::equal_to<>>
	#ifndef GAL_INI_UNORDERED_MAP_TYPE
	using unordered_map_type = std::unordered_map<Key, Value, KeyHash, KeyComparator>;
	#else
	using unordered_map_type = GAL_INI_UNORDERED_MAP_TYPE<Key, Value, KeyHash, KeyComparator>;
	#endif

	template<typename Value>
	using unordered_table_type = unordered_map_type<string_type, Value, string_hash_type>;

	template<typename Char>
	[[nodiscard]] consteval auto make_line_separator() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
			#ifdef GAL_INI_COMPILER_MSVC
			return L"\n";
			#else
			return L"\r\n";
			#endif
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
			#ifdef GAL_INI_COMPILER_MSVC
			return u8"\n";
			#else
			return u8"\r\n";
			#endif
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
			#ifdef GAL_INI_COMPILER_MSVC
			return u"\n";
			#else
			return u"\r\n";
			#endif
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
			#ifdef GAL_INI_COMPILER_MSVC
			return U"\n";
			#else
			return U"\r\n";
			#endif
		}
		else
		{
			#ifdef GAL_INI_COMPILER_MSVC
			return "\n";
			#else
			return "\r\n";
			#endif
		}
	}

	constexpr auto line_separator = make_line_separator<string_type::value_type>();
}

#include <ini/impl/ini_v3.hpp>

namespace gal::ini
{
	using impl::IniParser;
	using impl::IniParserWithComment;
}
