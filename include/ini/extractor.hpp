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

		using section_appender = FunctionRef<section_append_result(argument_type section_to_be_inserted)>;
	};

	// ①
	// appender

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
			appender_traits<string_view_type>::section_appender appender) -> ExtractResult;

	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			string_view_type                               buffer,
			appender_traits<string_type>::section_appender appender) -> ExtractResult;

	// ②
	// string_type + container

	template<typename String, typename Container>
		requires requires
		{
			// iterator -> key + mapped
			// #1
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<Container&>().end())>> == 2;

			// section insertion

			// iterator + bool
			// #2
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename Container::mapped_type>()))>> == 2;
			// iterator -> key + value
			// #3
			std::tuple_size_v<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename Container::mapped_type>()))>>> == 2;

			// kv insertion

			// iterator + bool
			// #4
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename appender_traits<String>::argument_type>()))>> == 2;
			// iterator -> key + value
			// #5
			std::tuple_size_v<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename appender_traits<String>::argument_type>()))>>> == 2;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename,
			Container&             out) -> ExtractResult
	{
		using traits_type = appender_traits<String>;

		// We need the following temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming `FunctionRef`.

		auto current_section_it = out.end();

		auto kv_appender = [&current_section_it](typename traits_type::argument_type key_to_be_inserted, typename traits_type::argument_type value_to_be_inserted) -> typename traits_type::kv_append_result
		{
			// see #1
			auto& [section_name, mapped] = *current_section_it;
			// see #4
			const auto [it, inserted] = mapped.emplace(std::forward<typename traits_type::argument_type>(key_to_be_inserted), std::forward<typename traits_type::argument_type>(value_to_be_inserted));
			// see #5
			const auto& [inserted_key, inserted_value] = *it;

			return {
					.key = inserted_key,
					.value = inserted_value,
					.inserted = inserted};
		};
		auto section_appender = [&out, &current_section_it, &kv_appender](typename traits_type::argument_type section_name) -> typename traits_type::section_append_result
		{
			// see #2
			const auto [it, inserted] = out.emplace(std::forward<typename traits_type::argument_type>(section_name), typename Container::mapped_type{});
			// see #3
			const auto& [inserted_name, inserted_mapped] = *it;

			current_section_it = it;
			return {
					.name = inserted_name,
					.appender = kv_appender,
					.inserted = inserted};
		};

		// call ①
		return extract_from_file(filename, section_appender);
	}

	template<typename String, typename Container>
		requires requires
		{
			{
				extract_from_file<String, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename) -> std::pair<ExtractResult, Container>
	{
		Container out{};
		auto      result = extract_from_file<String, Container>(filename, out);
		return {result, out};
	}

	template<typename String, typename Container>
		requires requires
		{
			// iterator -> key + mapped
			// #1
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<Container&>().end())>> == 2;

			// section insertion

			// iterator + bool
			// #2
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename Container::mapped_type>()))>> == 2;
			// iterator -> key + value
			// #3
			std::tuple_size_v<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<Container&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename Container::mapped_type>()))>>> == 2;

			// kv insertion

			// iterator + bool
			// #4
			std::tuple_size_v<std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename appender_traits<String>::argument_type>()))>> == 2;
			// iterator -> key + value
			// #5
			std::tuple_size_v<std::tuple_element_t<0, std::remove_cvref_t<decltype(std::declval<typename Container::mapped_type&>().emplace(std::declval<typename appender_traits<String>::argument_type>(), std::declval<typename appender_traits<String>::argument_type>()))>>> == 2;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer,
			Container&             out) -> ExtractResult
	{
		using traits_type = appender_traits<String>;

		// We need the following temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming `FunctionRef`.

		auto current_section_it = out.end();

		auto kv_appender = [&current_section_it](typename traits_type::argument_type key_to_be_inserted, typename traits_type::argument_type value_to_be_inserted) -> typename traits_type::kv_append_result
		{
			// see #1
			auto& [section_name, mapped] = *current_section_it;
			// see #4
			const auto [it, inserted] = mapped.emplace(std::forward<typename traits_type::argument_type>(key_to_be_inserted), std::forward<typename traits_type::argument_type>(value_to_be_inserted));
			// see #5
			const auto& [inserted_key, inserted_value] = *it;

			return {
					.key = inserted_key,
					.value = inserted_value,
					.inserted = inserted};
		};
		auto section_appender = [&out, &current_section_it, &kv_appender](typename traits_type::argument_type section_name) -> typename traits_type::section_append_result
		{
			// see #2
			const auto [it, inserted] = out.emplace(std::forward<typename traits_type::argument_type>(section_name), typename Container::mapped_type{});
			// see #3
			const auto& [inserted_name, inserted_mapped] = *it;

			current_section_it = it;
			return {
					.name = inserted_name,
					.appender = kv_appender,
					.inserted = inserted};
		};

		// call ①
		return extract_from_buffer(buffer, section_appender);
	}

	template<typename String, typename Container>
		requires requires
		{
			{
				extract_from_buffer<String, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer) -> std::pair<ExtractResult, Container>
	{
		Container out{};
		auto      result = extract_from_buffer<String, Container>(buffer, out);
		return {result, out};
	}

	// ③
	// container

	template<typename Container>
		requires requires
		{
			{
				extract_from_file<typename Container::key_type, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename,
			Container&             out) -> ExtractResult
	{
		// call ②
		return extract_from_file<typename Container::key_type, Container>(filename, out);
	}

	template<typename Container>
		requires requires
		{
			{
				extract_from_file<typename Container::key_type, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_file(
			const string_view_type filename) -> std::pair<ExtractResult, Container>
	{
		Container out{};
		auto      result = extract_from_file<typename Container::key_type, Container>(filename, out);
		return {result, out};
	}

	template<typename Container>
		requires requires
		{
			{
				extract_from_buffer<typename Container::key_type, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer,
			Container&             out) -> ExtractResult
	{
		// call ②
		return extract_from_buffer<typename Container::key_type, Container>(buffer, out);
	}

	template<typename Container>
		requires requires
		{
			{
				extract_from_buffer<typename Container::key_type, Container>(std::declval<string_view_type>(), std::declval<Container&>())
			} -> std::same_as<ExtractResult>;
		}
	GAL_INI_SYMBOL_EXPORT auto extract_from_buffer(
			const string_view_type buffer) -> std::pair<ExtractResult, Container>
	{
		Container out{};
		auto      result = extract_from_buffer<typename Container::key_type, Container>(buffer, out);
		return {result, out};
	}
}
