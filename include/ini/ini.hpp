#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#if defined(GAL_INI_COMPILER_MSVC)
	#define GAL_INI_UNREACHABLE() __assume(0)
#elif defined(GAL_INI_COMPILER_GNU) || defined(GAL_INI_COMPILER_CLANG) || defined(GAL_INI_COMPILER_APPLE_CLANG)
	#define GAL_INI_UNREACHABLE() __builtin_unreachable()
#else
	#define GAL_INI_UNREACHABLE() throw
#endif

#ifdef GAL_INI_COMPILER_MSVC
	#define GAL_INI_STRING_CONSTEXPR constexpr
#else
	#define GAL_INI_STRING_CONSTEXPR inline
#endif

namespace gal::ini
{
	using string_type	   = std::string;
	using char_type		   = string_type::value_type;
	using char_traits_type = string_type::traits_type;

	using string_view_type = std::basic_string_view<char_type, char_traits_type>;

	struct string_hash_type
	{
		using is_transparent = int;

		[[nodiscard]] auto operator()(const string_type& string) const noexcept -> std::size_t { return std::hash<string_type>{}(string); }

		[[nodiscard]] auto operator()(const string_view_type& string) const noexcept -> std::size_t { return std::hash<string_view_type>{}(string); }
	};

	template<typename Value>
	using unordered_table_type = std::unordered_map<string_type, Value, string_hash_type, std::equal_to<>>;
	template<typename Value>
	using unordered_table_view_type = std::unordered_map<string_view_type, Value, string_hash_type, std::equal_to<>>;

	enum class CommentIndication
	{
		INVALID,

		HASH_SIGN = '#',
		SEMICOLON = ';',
	};

	template<typename Char>
	[[nodiscard]] consteval auto make_line_separator() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
#ifdef GAL_INI_PLATFORM_WINDOWS
			return L"\n";
#else
			return L"\r\n";
#endif
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
#ifdef GAL_INI_PLATFORM_WINDOWS
			return u8"\n";
#else
			return u8"\r\n";
#endif
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
#ifdef GAL_INI_PLATFORM_WINDOWS
			return u"\n";
#else
			return u"\r\n";
#endif
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
#ifdef GAL_INI_PLATFORM_WINDOWS
			return U"\n";
#else
			return U"\r\n";
#endif
		}
		else
		{
#ifdef GAL_INI_PLATFORM_WINDOWS
			return "\n";
#else
			return "\r\n";
#endif
		}
	}

	template<typename Char>
	[[nodiscard]] consteval auto make_kv_separator() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
			return L"=";
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
			return u8"=";
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
			return u"=";
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
			return U"=";
		}
		else
		{
			return "=";
		}
	}

	template<typename Char>
	[[nodiscard]] consteval auto make_blank_separator() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
			return L" ";
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
			return u8" ";
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
			return u" ";
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
			return U" ";
		}
		else
		{
			return " ";
		}
	}

	template<typename Char>
	[[nodiscard]] consteval auto make_square_bracket() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
			return std::pair{L'[', L']'};
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
			return std::pair{u8'[', u8']'};
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
			return std::pair{u'[', u']'};
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
			return std::pair{U'[', U']'};
		}
		else
		{
			return std::pair{'[', ']'};
		}
	}

	template<typename Char, CommentIndication indication>
	[[nodiscard]] consteval auto make_comment_indication() noexcept
	{
		if constexpr (std::is_same_v<Char, wchar_t>)
		{
			if constexpr (indication == CommentIndication::HASH_SIGN)
			{
				return L'#';
			}
			else if constexpr (indication == CommentIndication::SEMICOLON)
			{
				return L';';
			}
			else
			{
				GAL_INI_UNREACHABLE();
			}
		}
		else if constexpr (std::is_same_v<Char, char8_t>)
		{
			if constexpr (indication == CommentIndication::HASH_SIGN)
			{
				return u8'#';
			}
			else if constexpr (indication == CommentIndication::SEMICOLON)
			{
				return u8';';
			}
			else
			{
				GAL_INI_UNREACHABLE();
			}
		}
		else if constexpr (std::is_same_v<Char, char16_t>)
		{
			if constexpr (indication == CommentIndication::HASH_SIGN)
			{
				return u'#';
			}
			else if constexpr (indication == CommentIndication::SEMICOLON)
			{
				return u';';
			}
			else
			{
				GAL_INI_UNREACHABLE();
			}
		}
		else if constexpr (std::is_same_v<Char, char32_t>)
		{
			if constexpr (indication == CommentIndication::HASH_SIGN)
			{
				return U'#';
			}
			else if constexpr (indication == CommentIndication::SEMICOLON)
			{
				return U';';
			}
			else
			{
				GAL_INI_UNREACHABLE();
			}
		}
		else
		{
			if constexpr (indication == CommentIndication::HASH_SIGN)
			{
				return '#';
			}
			else if constexpr (indication == CommentIndication::SEMICOLON)
			{
				return ';';
			}
			else
			{
				GAL_INI_UNREACHABLE();
			}
		}
	}

	constexpr auto line_separator				= make_line_separator<string_type::value_type>();
	constexpr auto kv_separator					= make_kv_separator<string_type::value_type>();
	constexpr auto blank_separator				= make_blank_separator<string_type::value_type>();
	constexpr auto square_bracket				= make_square_bracket<string_type::value_type>();
	constexpr auto comment_indication_hash_sign = make_comment_indication<string_type::value_type, CommentIndication::HASH_SIGN>();
	constexpr auto comment_indication_semicolon = make_comment_indication<string_type::value_type, CommentIndication::SEMICOLON>();

	struct comment_type;

	struct comment_view_type
	{
		CommentIndication							indication;
		string_view_type							comment;

		[[nodiscard]] constexpr auto				empty() const noexcept -> bool { return indication == CommentIndication::INVALID; }

		[[nodiscard]] constexpr auto				operator==(const comment_view_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto operator==(const comment_type& other) const noexcept -> bool;
	};

	struct comment_type
	{
		CommentIndication									   indication;
		string_type											   comment;

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto			   empty() const noexcept -> bool { return comment.empty(); }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR explicit(false) operator comment_view_type() const noexcept { return {indication, comment}; }

		[[nodiscard]] constexpr auto						   operator==(const comment_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }
	};

	GAL_INI_STRING_CONSTEXPR auto comment_view_type::operator==(const comment_type& other) const noexcept -> bool { return *this == other.operator comment_view_type(); }

	[[nodiscard]] constexpr auto					 make_comment_indication(decltype(comment_indication_hash_sign) indication) -> CommentIndication
	{
		// using type = std::underlying_type_t<CommentIndication>;
		// if (static_cast<type>(CommentIndication::HASH_SIGN) == static_cast<type>(indication)) { return CommentIndication::HASH_SIGN; }
		if (indication == comment_indication_hash_sign) { return CommentIndication::HASH_SIGN; }

		// if (static_cast<type>(CommentIndication::SEMICOLON) == static_cast<type>(indication)) { return CommentIndication::SEMICOLON; }
		if (indication == comment_indication_semicolon) { return CommentIndication::SEMICOLON; }

		return CommentIndication::INVALID;
	}

	[[nodiscard]] constexpr auto make_comment_indication(const CommentIndication indication) -> std::remove_cvref_t<decltype(comment_indication_hash_sign)>
	{
		if (indication == CommentIndication::HASH_SIGN) { return comment_indication_hash_sign; }

		if (indication == CommentIndication::SEMICOLON) { return comment_indication_semicolon; }

		GAL_INI_UNREACHABLE();
	}

	[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto make_comment(const CommentIndication indication, string_type&& comment) -> comment_type { return {.indication = indication, .comment = std::move(comment)}; }

	[[nodiscard]] constexpr auto				make_comment_view(const CommentIndication indication, const string_view_type comment) -> comment_view_type { return {.indication = indication, .comment = comment}; }

}// namespace gal::ini

#include <ini/impl/extractor.hpp>
#include <ini/impl/flusher.hpp>
#include <ini/impl/manager.hpp>

namespace gal::ini
{
	using impl::FileExtractResult;
	using impl::IniExtractor;
	using impl::IniExtractorWithComment;

	using impl::IniManager;
	using impl::IniManagerWithComment;

	using impl::FileFlushResult;
	using impl::IniFlusher;
	using impl::IniFlusherWithComment;
}// namespace gal::ini
