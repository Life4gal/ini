#pragma once

#include <string_view>
#include <type_traits>

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
	namespace common
	{
		template<typename T>
		struct char_type;

		template<typename String>
			requires requires { typename String::value_type; } || requires { std::declval<String>()[0]; }
		struct char_type<String>
		{
			using type = std::conditional_t<
					requires { typename String::value_type; },
					typename String::value_type,
					std::decay_t<decltype(std::declval<String>()[0])>>;

			static_assert(std::is_integral_v<type>);
		};

		template<typename T>
		using char_type_t = typename char_type<T>::type;

		template<typename T>
		struct char_traits;

		template<typename String>
			requires requires { typename String::traits_type; } || requires { std::declval<String>()[0]; }
		struct char_traits<String>
		{
			using type = std::conditional_t<
					requires { typename String::value_type; },
					typename String::traits_type,
					std::char_traits<char_type_t<String>>>;
		};

		template<typename T>
		using char_traits_t = typename char_traits<T>::type;

		template<typename T>
		struct map_allocator_type;

		template<
				template<typename>
				typename Allocator,
				typename Key,
				typename Value>
		struct map_allocator_type<Allocator<std::pair<const Key, Value>>>
		{
			template<typename NewKey, typename NewValue>
			using type = Allocator<std::pair<const NewKey, NewValue>>;
		};

		template<
				template<typename>
				typename Allocator,
				typename Key,
				typename Value>
		// There may be implementations of map that do not require the Key to be const. :)
		struct map_allocator_type<Allocator<std::pair<Key, Value>>>
		{
			template<typename NewKey, typename NewValue>
			using type = Allocator<std::pair<NewKey, NewValue>>;
		};

		template<
				template<typename>
				typename Allocator,
				template<typename, typename>
				typename Pair,
				typename Key,
				typename Value>
		struct map_allocator_type<Allocator<Pair<const Key, Value>>>
		{
			template<typename NewKey, typename NewValue>
			using type = Allocator<Pair<const NewKey, NewValue>>;
		};

		template<
				template<typename>
				typename Allocator,
				template<typename, typename>
				typename Pair,
				typename Key,
				typename Value>
		// There may be implementations of map that do not require the Key to be const. :)
		struct map_allocator_type<Allocator<Pair<Key, Value>>>
		{
			template<typename NewKey, typename NewValue>
			using type = Allocator<Pair<NewKey, NewValue>>;
		};

		template<typename T, typename NewKey, typename NewValue>
		using map_allocator_type_t = typename map_allocator_type<T>::template type<NewKey, NewValue>;

		template<typename T>
		struct map_hash_type;

		template<
				template<typename>
				typename Hash,
				typename Key>
		struct map_hash_type<Hash<Key>>
		{
			template<typename NewKey>
			using type = Hash<NewKey>;
		};

		template<typename T, typename NewKey>
		using map_hash_type_t = typename map_hash_type<T>::template type<NewKey>;

		template<typename T>
		struct map_comparator_type;

		template<
				template<typename>
				typename Comparator,
				typename Key>
		struct map_comparator_type<Comparator<Key>>
		{
			template<typename NewKey>
			using type = Comparator<NewKey>;
		};

		template<typename T, typename NewKey>
		using map_comparator_type_t = typename map_comparator_type<T>::template type<NewKey>;

		template<typename T>
		struct map_type;

		template<
				template<typename, typename, typename, typename, typename, typename...>
				typename Map,
				typename Key,
				typename Value,
				typename Hash,
				typename KeyComparator,
				typename Allocator,
				typename... NeverMind>
		struct map_type<Map<Key, Value, Hash, KeyComparator, Allocator, NeverMind...>>
		{
			template<typename NewKey, typename NewValue, typename NewHash = map_hash_type_t<Hash, NewKey>, typename NewKeyComparator = map_comparator_type_t<KeyComparator, NewKey>>
			using type = Map<NewKey, NewValue, NewHash, NewKeyComparator, map_allocator_type_t<Allocator, NewKey, NewValue>, NeverMind...>;
		};

		template<
				template<typename, typename, typename, typename, typename...>
				typename Map,
				typename Key,
				typename Value,
				typename Hash,
				typename Allocator,
				typename... NeverMind>
		struct map_type<Map<Key, Value, Hash, Allocator, NeverMind...>>
		{
			template<typename NewKey, typename NewValue, typename NewHash = map_hash_type_t<Hash, NewKey>>
			using type = Map<NewKey, NewValue, NewHash, map_allocator_type_t<Allocator, NewKey, NewValue>, NeverMind...>;
		};

		template<
				template<typename, typename, typename, typename...>
				typename Map,
				typename Key,
				typename Value,
				typename Allocator,
				typename... NeverMind>
		struct map_type<Map<Key, Value, Allocator, NeverMind...>>
		{
			template<typename NewKey, typename NewValue>
			using type = Map<NewKey, NewValue, map_allocator_type_t<Allocator, NewKey, NewValue>, NeverMind...>;
		};

		template<
				template<typename, typename, typename, typename...>
				typename Map,
				typename Key,
				typename Value,
				typename... NeverMind>
		struct map_type<Map<Key, Value, NeverMind...>>
		{
			template<typename NewKey, typename NewValue>
			using type = Map<NewKey, NewValue, NeverMind...>;
		};

		template<typename T, typename... Required>
		using map_type_t = typename map_type<T>::template type<Required...>;
	}// namespace common

	template<typename String>
	using string_view_t = std::basic_string_view<common::char_type_t<String>, common::char_traits_t<String>>;

	template<typename String>
	struct string_hash_type
	{
		using is_transparent   = int;

		using string_type	   = String;
		using string_view_type = string_view_t<String>;

		[[nodiscard]] auto operator()(const string_type& string) const noexcept -> std::size_t { return std::hash<string_type>{}(string); }

		[[nodiscard]] auto operator()(const string_view_type& string) const noexcept -> std::size_t { return std::hash<string_view_type>{}(string); }
	};

	namespace common
	{
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
	}// namespace common

	template<typename String>
	inline constexpr auto line_separator = common::make_line_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	inline constexpr auto kv_separator = common::make_kv_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	inline constexpr auto blank_separator = common::make_blank_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	inline constexpr auto square_bracket = common::make_square_bracket<typename string_view_t<String>::value_type>();

	enum class CommentIndication
	{
		INVALID,

		HASH_SIGN = '#',
		SEMICOLON = ';',
	};

	namespace common
	{
		template<typename Char, CommentIndication Indication>
		[[nodiscard]] consteval auto make_comment_indication() noexcept
		{
			if constexpr (std::is_same_v<Char, wchar_t>)
			{
				if constexpr (Indication == CommentIndication::HASH_SIGN)
				{
					return L'#';
				}
				else if constexpr (Indication == CommentIndication::SEMICOLON)
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
				if constexpr (Indication == CommentIndication::HASH_SIGN)
				{
					return u8'#';
				}
				else if constexpr (Indication == CommentIndication::SEMICOLON)
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
				if constexpr (Indication == CommentIndication::HASH_SIGN)
				{
					return u'#';
				}
				else if constexpr (Indication == CommentIndication::SEMICOLON)
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
				if constexpr (Indication == CommentIndication::HASH_SIGN)
				{
					return U'#';
				}
				else if constexpr (Indication == CommentIndication::SEMICOLON)
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
				if constexpr (Indication == CommentIndication::HASH_SIGN)
				{
					return '#';
				}
				else if constexpr (Indication == CommentIndication::SEMICOLON)
				{
					return ';';
				}
				else
				{
					GAL_INI_UNREACHABLE();
				}
			}
		}
	}// namespace common

	template<typename String>
	inline constexpr auto comment_indication_hash_sign = common::make_comment_indication<typename string_view_t<String>::value_type, CommentIndication::HASH_SIGN>();
	template<typename String>
	inline constexpr auto comment_indication_semicolon = common::make_comment_indication<typename string_view_t<String>::value_type, CommentIndication::SEMICOLON>();

	template<typename String>
	struct comment_type;

	template<typename String>
	struct comment_view_type
	{
		using string_type = String;

		CommentIndication							indication;
		string_view_t<string_type>					comment;

		[[nodiscard]] constexpr auto				empty() const noexcept -> bool { return indication == CommentIndication::INVALID; }

		[[nodiscard]] constexpr auto				operator==(const comment_view_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto operator==(const comment_type<string_type>& other) const noexcept -> bool;
	};

	template<typename String>
	struct comment_type
	{
		using string_type = String;

		CommentIndication									   indication;
		string_type											   comment;

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto			   empty() const noexcept -> bool { return comment.empty(); }

		[[nodiscard]] GAL_INI_STRING_CONSTEXPR explicit(false) operator comment_view_type<string_type>() const noexcept { return {indication, comment}; }

		[[nodiscard]] constexpr auto						   operator==(const comment_type& other) const noexcept -> bool { return indication == other.indication && comment == other.comment; }
	};

	template<typename String>
	GAL_INI_STRING_CONSTEXPR auto comment_view_type<String>::operator==(const comment_type<String>& other) const noexcept -> bool
	{
		return *this == other.operator comment_view_type();
	}

	template<typename String>
	[[nodiscard]] constexpr auto make_comment_indication(decltype(comment_indication_hash_sign<String>) indication) -> CommentIndication
	{
		if (indication == comment_indication_hash_sign<String>) { return CommentIndication::HASH_SIGN; }

		if (indication == comment_indication_semicolon<String>) { return CommentIndication::SEMICOLON; }

		return CommentIndication::INVALID;
	}

	template<typename String>
	[[nodiscard]] constexpr auto make_comment_indication(const CommentIndication indication) -> std::remove_cvref_t<decltype(comment_indication_hash_sign<String>)>
	{
		if (indication == CommentIndication::HASH_SIGN) { return comment_indication_hash_sign<String>; }

		if (indication == CommentIndication::SEMICOLON) { return comment_indication_semicolon<String>; }

		GAL_INI_UNREACHABLE();
	}

	template<typename String>
	[[nodiscard]] GAL_INI_STRING_CONSTEXPR auto make_comment(const CommentIndication indication, String&& comment) -> comment_type<String>
	{
		return {.indication = indication, .comment = std::forward<String>(comment)};
	}

	template<typename String>
	[[nodiscard]] constexpr auto make_comment_view(const CommentIndication indication, const string_view_t<String> comment) -> comment_view_type<String>
	{
		return {.indication = indication, .comment = comment};
	}

	template<typename T>
	class StackFunction;

	namespace common
	{
		template<typename T>
		struct is_stack_function : std::false_type
		{
		};

		template<typename T>
		struct is_stack_function<StackFunction<T>> : std::true_type
		{
		};

		template<typename T>
		constexpr static bool is_stack_function_v = is_stack_function<T>::value;
	}// namespace common

	template<typename Return, typename... Args>
	class StackFunction<Return(Args...)>
	{
		using result_type  = Return;

		using invoker_type = auto (*)(const char*, Args&&...) -> result_type;

	private:
		invoker_type invoker_;
		const char*	 data_;

		template<typename Functor>
		[[nodiscard]] constexpr static auto do_invoke(Functor* functor, Args&&... args) noexcept(noexcept((*functor)(std::forward<Args>(args)...)))
				-> result_type
		{
			return (*functor)(std::forward<Args>(args)...);
		}

	public:
		// really?
		constexpr StackFunction() noexcept
			: invoker_{nullptr},
			  data_{nullptr} {}

		template<typename Functor>
			requires(!common::is_stack_function_v<Functor>)
		constexpr explicit(false) StackFunction(const Functor& functor) noexcept
			: invoker_{reinterpret_cast<invoker_type>(do_invoke<Functor>)},
			  data_{reinterpret_cast<const char*>(&functor)}
		{
		}

		constexpr auto operator()(Args... args) noexcept(noexcept(invoker_(data_, std::forward<Args>(args)...))) -> result_type
		{
			// !!!no nullptr check!!!
			return invoker_(data_, std::forward<Args>(args)...);
		}
	};
}// namespace gal::ini
