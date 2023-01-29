#pragma once

#include <ini/group_accessor.hpp>

#if defined(GAL_INI_STD_STRING)
	#include <string>

	#if defined(GAL_INI_STD_MAP)
		#include <unordered_map>
	#endif
#endif

namespace gal::ini
{
#if 0// defined(GAL_INI_CUSTOM_STRING)
	/**
	 * @brief Specialize std::allocator<ini_char> to use a custom allocator, otherwise this would be equivalent to using std::allocator<ini_char::type>>.
	 */
	struct ini_char
	{
		using type = char;

		type value;
	};

	/**
	 * @brief Specialize std::allocator<ini_char> to use a custom allocator, otherwise this would be equivalent to using std::allocator<ini_wchar::type>>.
	 */
	struct ini_wchar
	{
		using type = wchar_t;

		type value;
	};

	/**
	 * @brief Specialize std::allocator<ini_char> to use a custom allocator, otherwise this would be equivalent to using std::allocator<ini_char8::type>>.
	 */
	struct ini_char8
	{
		using type = char8_t;

		type value;
	};

	/**
	 * @brief Specialize std::allocator<ini_char> to use a custom allocator, otherwise this would be equivalent to using std::allocator<ini_char16::type>>.
	 */
	struct ini_char16
	{
		using type = char16_t;

		type value;
	};

	/**
	 * @brief Specialize std::allocator<ini_char> to use a custom allocator, otherwise this would be equivalent to using std::allocator<ini_char32::type>>.
	 */
	struct ini_char32
	{
		using type = char32_t;

		type value;
	};

	// After we parse the ini file, you can (actually must, otherwise memory will leak :)) safely move them away (to your custom string type).
	using string_type	 = std::span<ini_char>;
	// After we parse the ini file, you can (actually must, otherwise memory will leak :)) safely move them away (to your custom string type).
	using string_w_type	 = std::span<ini_wchar>;
	// After we parse the ini file, you can (actually must, otherwise memory will leak :)) safely move them away (to your custom string type).
	using string_8_type	 = std::span<ini_char8>;
	// After we parse the ini file, you can (actually must, otherwise memory will leak :)) safely move them away (to your custom string type).
	using string_16_type = std::span<ini_char16>;
	// After we parse the ini file, you can (actually must, otherwise memory will leak :)) safely move them away (to your custom string type).
	using string_32_type = std::span<ini_char32>;
#endif

	namespace extractor_detail
	{
		template<typename T>
		class StackFunction;

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
				requires(!is_stack_function_v<Functor>)
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
	}// namespace extractor_detail

#if defined(GAL_INI_STD_STRING)
	using string_std_type	= std::basic_string<char>;
	using string_stdw_type	= std::basic_string<wchar_t>;
	using string_std8_type	= std::basic_string<char8_t>;
	using string_std16_type = std::basic_string<char16_t>;
	using string_std32_type = std::basic_string<char32_t>;

	#if defined(GAL_INI_STD_MAP)
	// key <-> value
	using group_std_type   = std::unordered_map<string_std_type, string_std_type, group_accessor_detail::string_hash_type<string_std_type>, std::equal_to<>>;
	using group_stdw_type  = group_accessor_detail::map_type_t<group_std_type, string_stdw_type, string_stdw_type, group_accessor_detail::string_hash_type<string_stdw_type>>;
	using group_std8_type  = group_accessor_detail::map_type_t<group_std_type, string_std8_type, string_std8_type, group_accessor_detail::string_hash_type<string_std8_type>>;
	using group_std16_type = group_accessor_detail::map_type_t<group_std_type, string_std16_type, string_std16_type, group_accessor_detail::string_hash_type<string_std16_type>>;
	using group_std32_type = group_accessor_detail::map_type_t<group_std_type, string_std32_type, string_std32_type, group_accessor_detail::string_hash_type<string_std32_type>>;

	static_assert(std::is_same_v<group_std_type::key_type, string_std_type>);
	static_assert(std::is_same_v<group_std_type::mapped_type, string_std_type>);

	static_assert(std::is_same_v<group_stdw_type::key_type, string_stdw_type>);
	static_assert(std::is_same_v<group_stdw_type::mapped_type, string_stdw_type>);

	static_assert(std::is_same_v<group_std8_type::key_type, string_std8_type>);
	static_assert(std::is_same_v<group_std8_type::mapped_type, string_std8_type>);

	static_assert(std::is_same_v<group_std16_type::key_type, string_std16_type>);
	static_assert(std::is_same_v<group_std16_type::mapped_type, string_std16_type>);

	static_assert(std::is_same_v<group_std32_type::key_type, string_std32_type>);
	static_assert(std::is_same_v<group_std32_type::mapped_type, string_std32_type>);

	// group_name <-> keys&values
	using context_std_type	 = group_accessor_detail::map_type_t<group_std_type, string_std_type, group_std_type>;
	using context_stdw_type	 = group_accessor_detail::map_type_t<group_stdw_type, string_stdw_type, group_stdw_type>;
	using context_std8_type	 = group_accessor_detail::map_type_t<group_std8_type, string_std8_type, group_std8_type>;
	using context_std16_type = group_accessor_detail::map_type_t<group_std16_type, string_std16_type, group_std16_type>;
	using context_std32_type = group_accessor_detail::map_type_t<group_std32_type, string_std32_type, group_std32_type>;

	static_assert(std::is_same_v<context_std_type::key_type, string_std_type>);
	static_assert(std::is_same_v<context_std_type::mapped_type, group_std_type>);

	static_assert(std::is_same_v<context_stdw_type::key_type, string_stdw_type>);
	static_assert(std::is_same_v<context_stdw_type::mapped_type, group_stdw_type>);

	static_assert(std::is_same_v<context_std8_type::key_type, string_std8_type>);
	static_assert(std::is_same_v<context_std8_type::mapped_type, group_std8_type>);

	static_assert(std::is_same_v<context_std16_type::key_type, string_std16_type>);
	static_assert(std::is_same_v<context_std16_type::mapped_type, group_std16_type>);

	static_assert(std::is_same_v<context_std32_type::key_type, string_std32_type>);
	static_assert(std::is_same_v<context_std32_type::mapped_type, group_std32_type>);
	#endif
#endif

	enum class ExtractResult
	{
		// The file was not found.
		FILE_NOT_FOUND,
		// The file cannot be opened.
		PERMISSION_DENIED,
		// An internal OS error, such as failure to read from the file.
		INTERNAL_ERROR,

		SUCCESS,
	};

	template<typename Char>
	using kv_append_type =
			extractor_detail::StackFunction<
					auto
					// pass new key, new value
					(std::basic_string_view<Char>, std::basic_string_view<Char>)
							// return inserted(or exists) key, value and insert result
							->std::pair<std::pair<std::basic_string_view<Char>, std::basic_string_view<Char>>, bool>>;

	template<typename Char>
	struct group_append_result
	{
		// inserted(or exists) group_name
		std::basic_string_view<Char> name;
		// kv insert handle
		kv_append_type<Char>		 kv_appender;
		// insert result
		bool						 inserted;
	};

	template<typename Char>
	using group_append_type =
			extractor_detail::StackFunction<
					auto
					// pass new group name
					(std::basic_string_view<Char> group_name)
							// group_append_result
							->group_append_result<Char>>;

	namespace extractor_detail
	{
		// ==============================================
		// This is a very bad design, we have to iterate through all possible character types,
		// even StackFunction is on the edge of UB,
		// but in order to minimize dependencies and allow the user to maximize customization of the type, this design seems to be the only option.
		// ==============================================

		// char
		[[nodiscard]] auto extract_from_file(
				std::string_view		file_path,
				group_append_type<char> group_appender) -> ExtractResult;

		// wchar_t
		// [[nodiscard]] auto extract_from_file(
		// 		std::string_view		   file_path,
		// 		group_append_type<wchar_t> group_appender) -> ExtractResult;

		// char8_t
		[[nodiscard]] auto extract_from_file(
				std::string_view		   file_path,
				group_append_type<char8_t> group_appender) -> ExtractResult;

		// char16_t
		[[nodiscard]] auto extract_from_file(
				std::string_view			file_path,
				group_append_type<char16_t> group_appender) -> ExtractResult;

		// char32_t
		[[nodiscard]] auto extract_from_file(
				std::string_view			file_path,
				group_append_type<char32_t> group_appender) -> ExtractResult;
	}// namespace extractor_detail

	/**
	 * @brief Extract ini data from files.
	 * @tparam ContextType Type of the output data.
	 * @param file_path The (absolute) path to the file.
	 * @param group_appender How to add a new group.
	 * @return Extract result.
	 */
	template<typename ContextType>
	auto extract_from_file(
			std::string_view																	  file_path,
			group_append_type<typename string_view_t<typename ContextType::key_type>::value_type> group_appender) -> ExtractResult
	{
		return extractor_detail::extract_from_file(
				file_path,
				group_appender);
	}

	/**
	 * @brief Extract ini data from files.
	 * @tparam ContextType Type of the output data.
	 * @param file_path The (absolute) path to the file.
	 * @param out Where the extracted data is stored.
	 * @return Extract result.
	 */
	template<typename ContextType>
	auto extract_from_file(std::string_view file_path, ContextType& out) -> ExtractResult
	{
		using context_type			 = std::remove_cvref_t<decltype(out)>;

		using key_type				 = context_type::key_type;
		using group_type			 = context_type::mapped_type;

		using group_key_type		 = group_type::key_type;
		using group_mapped_type		 = group_type::mapped_type;

		using char_type				 = typename string_view_t<key_type>::value_type;
		using this_kv_append_type	 = kv_append_type<char_type>;
		using this_group_append_type = group_append_type<char_type>;

		return extract_from_file(
				file_path,
				this_group_append_type{
						[&out](string_view_t<key_type> group_name) -> group_append_result<char_type>
						{
							const auto [group_it, group_inserted] = out.emplace(key_type{group_name}, group_type{});
							return {
									.name = group_it->first,
									.kv_appender =
											this_kv_append_type{
													[group_it](string_view_t<group_key_type> key, string_view_t<group_mapped_type> value) -> std::pair<std::pair<string_view_t<group_key_type>, string_view_t<group_mapped_type>>, bool>
													{
														const auto [kv_it, kv_inserted] = group_it->second.emplace(group_key_type{key}, group_mapped_type{value});
														return {{kv_it->first, kv_it->second}, kv_inserted};
													}},
									.inserted = group_inserted};
						}});
	}
}// namespace gal::ini
