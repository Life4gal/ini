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
		using char_type_t = char_type<T>::type;

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
		using char_traits_t = char_traits<T>::type;

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
		using map_allocator_type_t = map_allocator_type<T>::template type<NewKey, NewValue>;

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
			template<typename NewKey, typename NewValue, typename NewHash = Hash, typename NewKeyComparator = KeyComparator>
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
			template<typename NewKey, typename NewValue, typename NewHash = Hash>
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
		using map_type_t = map_type<T>::template type<Required...>;
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
	constexpr auto line_separator = common::make_line_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	constexpr auto kv_separator = common::make_kv_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	constexpr auto blank_separator = common::make_blank_separator<typename string_view_t<String>::value_type>();
	template<typename String>
	constexpr auto square_bracket = common::make_square_bracket<typename string_view_t<String>::value_type>();

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
	constexpr auto comment_indication_hash_sign = common::make_comment_indication<typename string_view_t<String>::value_type, CommentIndication::HASH_SIGN>();
	template<typename String>
	constexpr auto comment_indication_semicolon = common::make_comment_indication<typename string_view_t<String>::value_type, CommentIndication::SEMICOLON>();

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
