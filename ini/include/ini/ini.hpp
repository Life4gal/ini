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

#include <version>
#ifdef __has_cpp_attribute
	#if defined(__cpp_lib_constexpr_string) // __has_cpp_attribute(__cpp_lib_constexpr_string)
		#define GAL_INI_STRING_CONSTEXPR constexpr
	#else
		#define GAL_INI_STRING_CONSTEXPR inline
	#endif
#else
	#define GAL_INI_STRING_CONSTEXPR inline
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

	enum class CommentIndication
	{
		INVALID,

		HASH_SIGN = '#',
		SEMICOLON = ';',
	};

	struct comment_type;

	struct comment_view_type
	{
		CommentIndication indication;
		string_view_type  comment;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool { return indication == CommentIndication::INVALID; }

		[[nodiscard]] constexpr auto operator==(const comment_view_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto operator==(const comment_type& other) const noexcept -> bool;
	};

	struct comment_type
	{
		CommentIndication indication;
		string_type       comment;

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto empty() const noexcept -> bool { return comment.empty(); }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR explicit(false) operator comment_view_type() const noexcept { return {indication, comment}; }

		[[nodiscard]] constexpr auto operator==(const comment_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }
	};

	GAL_INI_STRING_CONSTEXPR auto comment_view_type::operator==(const comment_type& other) const noexcept -> bool { return *this == other.operator comment_view_type(); }

	[[nodiscard]] constexpr auto make_comment_indication(const char indication) -> CommentIndication
	{
		using type = std::underlying_type_t<CommentIndication>;
		if (static_cast<type>(CommentIndication::HASH_SIGN) == static_cast<type>(indication)) { return CommentIndication::HASH_SIGN; }

		if (static_cast<type>(CommentIndication::SEMICOLON) == static_cast<type>(indication)) { return CommentIndication::SEMICOLON; }

		return CommentIndication::INVALID;
	}

	[[nodiscard]] constexpr auto make_comment_indication(const CommentIndication indication) -> char
	{
		if (indication == CommentIndication::HASH_SIGN) { return '#'; }

		if (indication == CommentIndication::SEMICOLON) { return ';'; }

		GAL_INI_UNREACHABLE();
	}

	[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto make_comment(const CommentIndication indication, string_type&& comment) -> comment_type { return {.indication = indication, .comment = std::move(comment)}; }

	[[nodiscard]] constexpr auto make_comment_view(const CommentIndication indication, const string_view_type comment) -> comment_view_type { return {.indication = indication, .comment = comment}; }

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

#undef GAL_INI_STRING_CONSTEXPR
