#pragma once

#include <ini/internal/common.hpp>

namespace gal::ini
{
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
	StackFunction<
		#if not defined(GAL_INI_COMPILER_MSVC)
		auto
		// pass new key, new value
		(string_view_t<Char>,
		string_view_t<Char>)
		// return inserted(or exists) key, value and insert result
			-> std::pair<std::pair<string_view_t<Char>, string_view_t<Char>>, bool>
			#else
		std::pair<std::pair<string_view_t<Char>, string_view_t<Char>>, bool>
		(string_view_t<Char>, string_view_t<Char>)
			#endif
	>;

	template<typename Char>
	struct group_append_result
	{
		// inserted(or exists) group_name
		string_view_t<Char> name;
		// kv insert handle
		kv_append_type<Char> kv_appender;
		// insert result
		bool inserted;
	};

	template<typename Char>
	using group_append_type =
	StackFunction<
		#if not defined(GAL_INI_COMPILER_MSVC)
		auto
		// pass new group name
		(string_view_t<Char> group_name)
		// group_append_result
			-> group_append_result<Char>
			#else
		group_append_result<Char>
		(string_view_t<Char> group_name)
			#endif
	>;

	namespace extractor_detail
	{
		// ==============================================
		// This is a very bad design, we have to iterate through all possible character types,
		// even StackFunction is on the edge of UB,
		// but in order to minimize dependencies and allow the user to maximize customization of the type, this design seems to be the only option.
		// ==============================================

		// ====================================================
		// For extract from files, we support four character types and assume the encoding of the file based on the character type.
		// ====================================================

		// char
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_file(
				std::string_view        file_path,
				group_append_type<char> group_appender) -> ExtractResult;

		// char8_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_file(
				std::string_view           file_path,
				group_append_type<char8_t> group_appender) -> ExtractResult;

		// char16_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_file(
				std::string_view            file_path,
				group_append_type<char16_t> group_appender) -> ExtractResult;

		// char32_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_file(
				std::string_view            file_path,
				group_append_type<char32_t> group_appender) -> ExtractResult;

		// ====================================================
		// For extract from buffer, we support four character types and assume the encoding of the file based on the character type.
		// ====================================================

		// char
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
				string_view_t<char>     buffer,
				group_append_type<char> group_appender) -> ExtractResult;

		// char8_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
				string_view_t<char8_t>     buffer,
				group_append_type<char8_t> group_appender) -> ExtractResult;

		// char16_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
				string_view_t<char16_t>     buffer,
				group_append_type<char16_t> group_appender) -> ExtractResult;

		// char32_t
		[[nodiscard]] GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
				string_view_t<char32_t>     buffer,
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
			const std::string_view                                                                file_path,
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
	auto extract_from_file(const std::string_view file_path, ContextType& out) -> ExtractResult
	{
		using context_type = ContextType;

		using key_type = typename context_type::key_type;
		using group_type = typename context_type::mapped_type;

		using group_key_type = typename group_type::key_type;
		using group_mapped_type = typename group_type::mapped_type;

		using char_type = typename string_view_t<key_type>::value_type;

		// We need the following one temporary variable to hold some necessary information, and they must have a longer lifetime than the incoming StackFunction.
		auto current_group_it = out.end();

		// !!!MUST PLACE HERE!!!
		// StackFunction keeps the address of the lambda and forwards the argument to the lambda when StackFunction::operator() has been called.
		// This requires that the lambda "must" exist at this point (i.e. have a longer lifecycle than the StackFunction), which is fine for a single-level lambda (maybe?).
		// However, if there is nesting, then the lambda will end its lifecycle early and the StackFunction will refer to an illegal address.
		// Walking on the edge of UB!
		auto kv_appender = [&current_group_it](const string_view_t<group_key_type> key, const string_view_t<group_mapped_type> value) -> std::pair<std::pair<string_view_t<group_key_type>, string_view_t<group_mapped_type>>, bool>
		{
			const auto [kv_it, kv_inserted] = current_group_it->second.emplace(group_key_type{key}, group_mapped_type{value});
			return {{kv_it->first, kv_it->second}, kv_inserted};
		};

		return extract_from_file<ContextType>(
				file_path,
				group_append_type<char_type>{
						[&out, &current_group_it, &kv_appender](string_view_t<key_type> group_name) -> group_append_result<char_type>
						{
							#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
							const auto workaround_emplace_result = out.emplace(key_type{group_name}, group_type{});
							const auto group_it                  = workaround_emplace_result.first;
							const auto group_inserted            = workaround_emplace_result.second;
							#else
							const auto [group_it, group_inserted] = out.emplace(key_type{group_name}, group_type{});
							#endif

							current_group_it = group_it;

							return {
									.name = group_it->first,
									.kv_appender = kv_appender,
									.inserted = group_inserted};
						}});
	}

	template<typename ContextType>
	auto extract_from_file(const std::string_view file_path) -> std::pair<ExtractResult, ContextType>
	{
		ContextType out{};
		const auto  result = extract_from_file<ContextType>(file_path, out);
		return {result, out};
	}

	/**
	 * @brief Extract ini data from buffer.
	 * @tparam ContextType Type of the output data.
	 * @param buffer The buffer.
	 * @param group_appender How to add a new group.
	 * @return Extract result.
	 */
	template<typename ContextType>
	auto extract_from_buffer(
			string_view_t<typename string_view_t<typename ContextType::key_type>::value_type>     buffer,
			group_append_type<typename string_view_t<typename ContextType::key_type>::value_type> group_appender) -> ExtractResult
	{
		return extractor_detail::extract_from_buffer(
				buffer,
				group_appender);
	}

	/**
	 * @brief Extract ini data from buffer.
	 * @tparam ContextType Type of the output data.
	 * @param buffer The buffer.
	 * @param out Where the extracted data is stored.
	 * @return Extract result.
	 */
	template<typename ContextType>
	auto extract_from_buffer(
			string_view_t<typename string_view_t<typename ContextType::key_type>::value_type> buffer,
			ContextType&                                                                      out) -> ExtractResult
	{
		using context_type = ContextType;

		using key_type = typename context_type::key_type;
		using group_type = typename context_type::mapped_type;

		using group_key_type = typename group_type::key_type;
		using group_mapped_type = typename group_type::mapped_type;

		using char_type = typename string_view_t<key_type>::value_type;

		// We need the following one temporary variable to hold some necessary information, and they must have a longer lifetime than the incoming StackFunction.
		typename context_type::iterator current_group_it = out.end();

		// !!!MUST PLACE HERE!!!
		// StackFunction keeps the address of the lambda and forwards the argument to the lambda when StackFunction::operator() has been called.
		// This requires that the lambda "must" exist at this point (i.e. have a longer lifecycle than the StackFunction), which is fine for a single-level lambda (maybe?).
		// However, if there is nesting, then the lambda will end its lifecycle early and the StackFunction will refer to an illegal address.
		// Walking on the edge of UB!
		auto kv_appender = [&current_group_it](const string_view_t<group_key_type> key, const string_view_t<group_mapped_type> value) -> std::pair<std::pair<string_view_t<group_key_type>, string_view_t<group_mapped_type>>, bool>
		{
			const auto [kv_it, kv_inserted] = current_group_it->second.emplace(group_key_type{key}, group_mapped_type{value});
			return {{kv_it->first, kv_it->second}, kv_inserted};
		};

		return extract_from_buffer<ContextType>(
				buffer,
				group_append_type<char_type>{
						[&out, &current_group_it, &kv_appender](string_view_t<key_type> group_name) -> group_append_result<char_type>
						{
							#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
							const auto workaround_emplace_result = out.emplace(key_type{group_name}, group_type{});
							const auto group_it                  = workaround_emplace_result.first;
							const auto group_inserted            = workaround_emplace_result.second;
							#else
							const auto [group_it, group_inserted] = out.emplace(key_type{group_name}, group_type{});
							#endif

							current_group_it = group_it;

							return {
									.name = group_it->first,
									.kv_appender = kv_appender,
									.inserted = group_inserted};
						}});
	}

	template<typename ContextType>
	auto extract_from_buffer(
			string_view_t<typename string_view_t<typename ContextType::key_type>::value_type> buffer) -> std::pair<ExtractResult, ContextType>
	{
		ContextType out{};
		const auto  result = extract_from_buffer<ContextType>(buffer, out);
		return {result, out};
	}
}// namespace gal::ini
