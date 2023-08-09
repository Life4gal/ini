// Copyright (C) 2022-2023 Life4gal <life4gal@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#pragma once

#include <ini/impl/function_ref.hpp>
#include <ini/impl/common.hpp>

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

	template<typename String>
	struct appender_traits
	{
		using string_type = String;
		using char_type = typename string_type::value_type;

		constexpr static auto allocatable = requires(string_type& string) { string.push_back(std::declval<char_type>()); };

		using view_type = std::conditional_t<allocatable, std::basic_string_view<char_type>, String>;
		using argument_type = std::conditional_t<allocatable, std::add_rvalue_reference_t<string_type>, string_type>;

		GAL_PROMETHEUS_DISABLE_WARNING_PUSH
		GAL_PROMETHEUS_DISABLE_WARNING_GNU(-Wpadded)
		GAL_PROMETHEUS_DISABLE_WARNING_CLANG(-Wpadded)
		GAL_PROMETHEUS_DISABLE_WARNING_MSVC(4123)

		struct kv_append_result
		{
			// The key in the container
			view_type key;
			// The value in the container
			view_type value;
			// Newly inserted property?
			bool inserted;
		};

		using kv_appender = FunctionRef<kv_append_result(argument_type key_to_be_inserted, argument_type value_to_be_inserted)>;

		struct section_append_result
		{
			// The name of the current section
			view_type name;
			// kv appender
			kv_appender appender;
			// Newly inserted section?
			bool inserted;
		};

		GAL_PROMETHEUS_DISABLE_WARNING_POP

		using section_appender = FunctionRef<section_append_result(argument_type section_name_to_be_inserted)>;
	};

	// ①
	// The implementation of the template function must be visible in all translation unit, so we must specialize two implementations of the appender type.
	// One of them is string_type (usually std::string), which allocates memory when generating strings and handles Unicode characters when parsing.
	// Another of them is string_view_type (usually std::string_view), which does not allocate memory when generating strings, meaning that the contents of the string are parsed as is.
	// If the user's string type is the same as string_type or can be constructed from string_type (rvalue string_type), appender_traits<string_type>::section_appender is used.
	// If the user's string type is the same as string_view_type or can be constructed from string_view_type, appender_traits<string_view_type>::section_appender is used.
	// If neither, the implementation will not exist and the link will generate an error.

	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			string_view_type                                    filename,
			appender_traits<string_view_type>::section_appender appender
			) -> ExtractResult;

	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			string_view_type                               filename,
			appender_traits<string_type>::section_appender appender
			) -> ExtractResult;

	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			string_view_type                                    buffer,
			appender_traits<string_view_type>::section_appender appender
			) -> ExtractResult;

	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			string_view_type                               buffer,
			appender_traits<string_type>::section_appender appender
			) -> ExtractResult;

	// ②
	// Typically the user will call the following overloads.
	// It is assumed that the container type is similar to std::map<user_string, std::map<user_string, user_string>> and that the insertion operations have similar behavior and return values.

	namespace extractor_detail
	{
		enum class Source
		{
			FILE,
			BUFFER,
		};

		template<Source S, typename Container>
			requires requires
			{
				// iterator -> key + mapped
				// #1
				std::tuple_size_v<std::remove_cvref_t<decltype(*std::declval<Container&>().end())>> == 2;

				// section insertion

				// iterator + bool
				// #2
				std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename Container::key_type>(), std::declval<typename Container::mapped_type>()))>> == 2;
				// iterator -> key + value
				// #3
				std::tuple_size_v<std::remove_cvref_t<decltype(*std::declval<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename Container::key_type>(), std::declval<typename Container::mapped_type>()))>>>())>> == 2;

				// kv insertion

				// iterator + bool
				// #4
				std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename Container::mapped_type::key_type>(), std::declval<typename Container::mapped_type::mapped_type>()))>> == 2;
				// iterator -> key + value
				// #5
				std::tuple_size_v<std::remove_cvref_t<decltype(*std::declval<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename Container::mapped_type::key_type>(), std::declval<typename Container::mapped_type::mapped_type>()))>>>())>> == 2;
			}
		auto call_extract(
				const string_view_type filename_or_buffer,
				Container&             out
				) -> ExtractResult
		{
			using user_string_type = typename Container::key_type;
			using user_traits_type = appender_traits<user_string_type>;

			// We can't use traits_type directly, because the user's string type may not be string_type or string_view_type.
			// Depending on whether the user's string type is allocatable or not, we can decide whether the parameter type of the later FunctionRef is string_type or string_view_type, and then construct the user's string from string_type/string_view_type.
			using traits_type = appender_traits<std::conditional_t<user_traits_type::allocatable, string_type, string_view_type>>;

			// We need the following temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming `FunctionRef`.

			auto current_section_it = out.end();

			auto kv_appender = [&current_section_it](typename traits_type::argument_type key_to_be_inserted, typename traits_type::argument_type value_to_be_inserted) -> typename traits_type::kv_append_result
			{
				// see #1
				auto&            [section_name, mapped] = *current_section_it;
				user_string_type key{std::forward<typename traits_type::argument_type>(key_to_be_inserted)};
				user_string_type value{std::forward<typename traits_type::argument_type>(value_to_be_inserted)};
				// see #4
				const auto [it, inserted] = mapped.emplace(std::move(key), std::move(value));
				// see #5
				const auto& [inserted_key, inserted_value] = *it;

				return {
						.key = inserted_key,
						.value = inserted_value,
						.inserted = inserted};
			};
			auto section_appender = [&out, &current_section_it, &kv_appender](typename traits_type::argument_type section_name_to_be_inserted) -> typename traits_type::section_append_result
			{
				user_string_type section_name{std::forward<typename traits_type::argument_type>(section_name_to_be_inserted)};
				// see #2
				const auto [it, inserted] = out.emplace(std::move(section_name), typename Container::mapped_type{});
				// see #3
				const auto& [inserted_name, inserted_mapped] = *it;

				current_section_it = it;
				return {
						.name = inserted_name,
						.appender = kv_appender,
						.inserted = inserted};
			};

			if constexpr (S == Source::FILE) { return extract_from_file(filename_or_buffer, section_appender); }
			else if constexpr (S == Source::BUFFER) { return extract_from_buffer(filename_or_buffer, section_appender); }
			else
			{
				[]<bool AlwaysFalse = false>() { static_assert(AlwaysFalse); }();
				return ExtractResult::INTERNAL_ERROR;
			}
		}
	}

	template<typename Container>
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename,
			Container&             out
			) -> ExtractResult requires requires
	{
		{
			extractor_detail::call_extract<extractor_detail::Source::FILE>(filename, out)
		} -> std::same_as<ExtractResult>;
	}
	{
		// call ①
		return extractor_detail::call_extract<extractor_detail::Source::FILE>(filename, out);
	}

	template<typename Container>
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename
			) -> std::pair<ExtractResult, Container> requires requires
	{
		{
			extract_from_file<Container>(filename, std::declval<Container&>())
		} -> std::same_as<ExtractResult>;
	}
	{
		Container out{};
		auto      result = extract_from_file<Container>(filename, out);
		return {result, out};
	}

	template<typename Container>
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer,
			Container&             out
			) -> ExtractResult requires requires
	{
		{
			extractor_detail::call_extract<extractor_detail::Source::BUFFER>(buffer, out)
		} -> std::same_as<ExtractResult>;
	}
	{
		// call ①
		return extractor_detail::call_extract<extractor_detail::Source::BUFFER>(buffer, out);
	}

	template<typename Container>
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer
			) -> std::pair<ExtractResult, Container> requires requires
	{
		{
			extract_from_buffer<Container>(buffer, std::declval<Container&>())
		} -> std::same_as<ExtractResult>;
	}
	{
		Container out{};
		auto      result = extract_from_buffer<Container>(buffer, out);
		return {result, out};
	}
}
