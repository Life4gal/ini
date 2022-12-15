#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ini/ini.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>
#include <ranges>

namespace
{
	namespace ini = gal::ini;

	template<typename StringType>
	[[nodiscard]] auto to_char_string(const StringType& string) -> decltype(auto)
	{
		if constexpr (std::is_same_v<typename StringType::value_type, char>)
		{
			return std::string_view{string.data(), string.size()};
		}
		else
		{
			return std::filesystem::path{string.data(), string.data() + string.size()}.string();
		}
	}

	template<typename Impl, typename Ini, typename Encoding>
	class ParseState
	{
		using impl_type = Impl;

		[[nodiscard]] constexpr auto rep() const -> const impl_type&
		{
			return static_cast<const impl_type&>(*this);
		}

		[[nodiscard]] constexpr auto rep() -> impl_type&
		{
			return static_cast<impl_type&>(*this);
		}

	public:
		template<typename Return, typename... Functions>
		[[nodiscard]] constexpr static auto callback(Functions&&... functions)
		{
			return lexy::bind(
					lexy::callback<Return>(std::forward<Functions>(functions)...),
					// out parse state
					lexy::parse_state,
					// parsed values
					lexy::values);
		}

		using encoding	  = Encoding;
		using buffer_type = lexy::buffer<encoding>;
		using char_type	  = buffer_type::char_type;

		static_assert(std::is_same_v<encoding, typename buffer_type::encoding>);

		using ini_type	  = Ini;
		using writer_type = ini_type::writer_type;
		using group_type  = ini_type::group_type;

	private:
		const buffer_type&						 buffer_;
		lexy::input_location_anchor<buffer_type> buffer_anchor_;

		ini::filename_view_type					 filename_;

	protected:
		ini_type&					 ini_;
		std::unique_ptr<writer_type> writer_;

		auto						 report_duplicate_declaration(const char_type* position, const ini::string_view_type identifier, const ini::string_view_type category) const -> void
		{
			const auto						  location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			const auto						  out	   = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{buffer_, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(out,
									   lexy_ext::diagnostic_kind::error,
									   [&](lexy::cfile_output_iterator, lexy::visualization_options)
									   {
										   (void)std::fprintf(stderr, "duplicate %s declaration named '%s'", to_char_string(category).data(), to_char_string(identifier).data());
										   return out;
									   });

			if (!filename_.empty()) { (void)writer.write_path(out, filename_.data()); }

			(void)writer.write_empty_annotation(out);
			(void)writer.write_annotation(
					out,
					lexy_ext::annotation_kind::primary,
					location,
					identifier.size(),
					[&](lexy::cfile_output_iterator, lexy::visualization_options)
					{
						(void)std::fprintf(stderr, "second declaration here");
						return out;
					});
		}

		auto debug_print_variable(const char_type* position, const ini::string_view_type key, const ini::string_view_type value) const -> void
		{
			const auto						  location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			const auto						  out	   = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{buffer_, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(out,
									   lexy_ext::diagnostic_kind::info,
									   [&](lexy::cfile_output_iterator, lexy::visualization_options)
									   {
										   (void)std::fprintf(stderr, "[%s]: %s = %s", to_char_string(writer_->name()).data(), to_char_string(key).data(), to_char_string(value).data());
										   return out;
									   });

			if (!filename_.empty()) { (void)writer.write_path(out, filename_.data()); }

			(void)writer.write_empty_annotation(out);
			(void)writer.write_annotation(
					out,
					lexy_ext::annotation_kind::primary,
					location,
					key.size() + value.size(),
					[&](lexy::cfile_output_iterator, lexy::visualization_options)
					{
						(void)std::fprintf(stderr, "at here.");
						return out;
					});
		}

	public:
		ParseState(
				const ini::filename_view_type filename,
				const buffer_type&			  buffer,
				ini_type&					  ini)
			: buffer_{buffer},
			  buffer_anchor_{buffer_},
			  filename_{filename},
			  ini_{ini},
			  writer_{nullptr} {}

		auto begin_group(const char_type* position, ini::string_type&& group_name) -> void
		{
			rep().do_begin_group(position, std::move(group_name));
		}

		auto value(const char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
		{
			rep().do_value(position, std::move(key), std::move(value));
		}
	};

	template<typename Encoding>
	class UnorderedParseState final : public ParseState<UnorderedParseState<Encoding>, ini::impl::IniReader, Encoding>
	{
		using parent = ParseState<UnorderedParseState<Encoding>, ini::impl::IniReader, Encoding>;
		friend parent;

	public:
		using ParseState<UnorderedParseState<Encoding>, ini::impl::IniReader, Encoding>::ParseState;

	private:
		auto do_begin_group(const typename parent::char_type* position, ini::string_type&& group_name) -> void
		{
			// [[maybe_unused]] const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			this->writer_ = std::make_unique<typename parent::writer_type>(this->ini_.write(std::move(group_name)));


			if (!this->writer_->empty())
			{
				// If we get here, it means that a group with the same name already exists before, then this 'group_name' will not be consumed because of move.
				this->report_duplicate_declaration(position, group_name, "group");
			}
		}

		auto do_value(const typename parent::char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
		{
			// [[maybe_unused]] const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			// Our parse ensures the writer is valid
			if (!this->writer_->try_insert(std::move(key), std::move(value)))
			{
				// If we get here, it means that a key with the same name already exists before, then this 'key' will not be consumed because of move.
				this->report_duplicate_declaration(position, key, "variable");
			}
		}
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		template<typename ParseState>
		struct identifier
		{
			constexpr static auto rule =
					dsl::identifier(
							// begin with alpha/digit/underscore
							dsl::ascii::alpha_digit_underscore,
							// continue with alpha/digit/underscore
							dsl::ascii::alpha_digit_underscore);

			constexpr static auto value = lexy::as_string<ini::string_type, typename ParseState::encoding>;
		};

		template<typename ParseState>
		struct variable
		{
			// struct invalid_char
			// {
			// 	static LEXY_CONSTEVAL auto name() { return "invalid character in string literal"; }
			// };
			//
			// // A mapping of the simple escape sequences to their replacement values.
			// static constexpr auto escaped_symbols = lexy::symbol_table<char>//
			// 												.map<'"'>('"')
			// 												.map<'\\'>('\\')
			// 												.map<'/'>('/')
			// 												.map<'b'>('\b')
			// 												.map<'f'>('\f')
			// 												.map<'n'>('\n')
			// 												.map<'r'>('\r')
			// 												.map<'t'>('\t');
			//
			// struct code_point_id
			// {
			// 	// We parse the integer value of a UTF-16 code unit.
			// 	static constexpr auto rule	= LEXY_LIT("u") >> dsl::code_unit_id<lexy::utf16_encoding, 4>;
			// 	// And convert it into a code point, which might be a surrogate.
			// 	static constexpr auto value = lexy::construct<lexy::code_point>;
			// };
			//
			// static constexpr auto rule = []
			// {
			// 	// Everything is allowed inside a string except for control characters.
			// 	auto code_point = (-dsl::unicode::control).error<invalid_char>;
			//
			// 	// Escape sequences start with a backlash and either map one of the symbols,
			// 	// or a Unicode code point.
			// 	auto escape		= dsl::backslash_escape.symbol<escaped_symbols>().rule(dsl::p<code_point_id>);
			//
			// 	// String of code_point with specified escape sequences, surrounded by ".
			// 	// We abort string parsing if we see a newline to handle missing closing ".
			// 	return dsl::quoted.limit(dsl::ascii::newline)(code_point, escape);
			// }();

			constexpr static auto rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n' and '\r\n'
							dsl::unicode::print - dsl::unicode::newline);

			constexpr static auto value = lexy::as_string<ini::string_type, typename ParseState::encoding>;
		};

		// identifier = [variable]
		template<typename ParseState>
		struct variable_declaration
		{
			constexpr static auto rule =
					dsl::position +
					dsl::p<identifier<ParseState>> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable<ParseState>>);

			constexpr static auto value = ParseState::template callback<void>(
					// identifier = variable
					[](ParseState& state, const typename ParseState::char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
					{ state.value(position, std::move(key), std::move(value)); },
					// identifier =
					[](ParseState& state, const typename ParseState::char_type* position, ini::string_type&& key, lexy::nullopt) -> void
					{ state.value(position, std::move(key), ini::string_type{}); });
		};

		// [group_name]
		template<typename ParseState>
		struct group_declaration
		{
			struct header : lexy::transparent_production
			{
				constexpr static auto rule =
						// [
						dsl::square_bracketed.open() >>
						(dsl::position +
						 // group name
						 dsl::p<identifier<ParseState>> +
						 // ]
						 dsl::square_bracketed.close());

				constexpr static auto value = ParseState::template callback<void>(
						[](ParseState& state, const typename ParseState::char_type* position, ini::string_type&& group_name) -> void
						{ state.begin_group(position, std::move(group_name)); });
			};

			constexpr static auto rule =
					dsl::p<header> +
					// dsl::unicode::newline +
					// variables
					// dsl::recurse<variable_declaration>;
					// dsl::recurse_branch<variable_declaration>;

					// dsl::list(dsl::p<variable_declaration>, dsl::sep(dsl::unicode::newline));
					// dsl::list(dsl::p<variable_declaration>);

					// dsl::terminator(dsl::eof | dsl::square_bracketed.open()).opt_list(dsl::p<variable_declaration>, dsl::sep(dsl::unicode::newline));
					dsl::terminator(dsl::eof | dsl::peek(dsl::square_bracketed.open())).opt_list(dsl::p<variable_declaration<ParseState>>);

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename ParseState>
		struct file
		{
			constexpr static auto whitespace =
					// space
					dsl::ascii::blank |
					// The newline character is treated as a whitespace here, allowing us to skip the newline character, but this also leads to our above branching rules can no longer rely on the newline character.
					dsl::unicode::newline |
					// comment
					dsl::hash_sign >> dsl::until(dsl::newline);

			constexpr static auto rule	= dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<ParseState>>);

			constexpr static auto value = lexy::forward<void>;
		};
	}// namespace grammar
}// namespace

namespace gal::ini::impl
{
	namespace detail
	{
		auto GroupAccessor<GroupProperty::READ_MODIFY>::insert_or_assign(node_type&& node) -> bool
		{
			auto&& [key, value]		  = std::move(node);
			const auto [it, inserted] = group_.insert_or_assign(std::move(key), std::move(value));
			return inserted;
		}

		auto GroupAccessor<GroupProperty::READ_ORDERED>::get_it(group_type& group, string_view_type key) -> group_type::iterator
		{
			const auto it = std::ranges::find(
					group | std::views::values,
					key,
					[](const auto& pair) -> const auto&
					{
						return pair.first;
					});

			return it.base();
		}

		auto GroupAccessor<GroupProperty::READ_ORDERED>::get_it(const group_type& group, string_view_type key) -> group_type::const_iterator
		{
			return get_it(const_cast<group_type&>(group), key);
		}

		auto GroupAccessor<GroupProperty::READ_ORDERED>::contains(const string_view_type key) const -> bool
		{
			return get_it(group_, key) != group_.end();
		}

		auto GroupAccessor<GroupProperty::READ_ORDERED>::get(string_view_type key) const -> string_view_type
		{
			if (const auto it = get_it(group_, key);
				it != group_.end())
			{
				return it->second.second;
			}

			return {};
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert(const string_type& key, string_type&& value) -> bool
		{
			// try to find it
			if (const auto it = read_accessor_type::get_it(group_, key);
				it != group_.end())
			{
				// found it, ignore it
				return false;
			}
			else
			{
				group_.emplace(static_cast<line_type>(size()), group_type::mapped_type{key, std::move(value)});
				return true;
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert(string_type&& key, string_type&& value) -> bool
		{
			// try to find it
			if (const auto it = read_accessor_type::get_it(group_, key);
				it != group_.end())
			{
				// found it, ignore it
				return false;
			}
			else
			{
				group_.emplace(static_cast<line_type>(size()), group_type::mapped_type{std::move(key), std::move(value)});
				return true;
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert(node_type&& node) -> bool
		{
			// try to find it
			if (const auto it = read_accessor_type::get_it(group_, node.key());
				it != group_.end())
			{
				// found it, ignore it
				return false;
			}
			else
			{
				return group_.insert(std::move(node)) != group_.end();
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign(const string_type& key, string_type&& value) -> bool
		{
			// try to find it
			if (auto it = read_accessor_type::get_it(group_, key);
				it != group_.end())
			{
				// found it, assign it
				it->second.second = std::move(value);
				return false;
			}
			else
			{
				group_.emplace(static_cast<line_type>(size()), group_type::mapped_type{key, std::move(value)});
				return true;
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign(string_type&& key, string_type&& value) -> bool
		{
			// try to find it
			if (auto it = read_accessor_type::get_it(group_, key);
				it != group_.end())
			{
				// found it, assign it
				it->second.second = std::move(value);
				return false;
			}
			else
			{
				group_.emplace(static_cast<line_type>(size()), group_type::mapped_type{std::move(key), std::move(value)});
				return true;
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign(node_type&& node) -> bool
		{
			// try to find it
			if (auto it = read_accessor_type::get_it(group_, node.key());
				it != group_.end())
			{
				// found it, assign it
				it->second.second = std::move(node).value();
				return false;
			}
			else
			{
				return group_.insert(std::move(node)) != group_.end();
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::get_it(string_view_type target_key, string_view_type key) -> std::pair<group_type::iterator, group_type::iterator>
		{
			// try to find target key
			if (auto target_it = read_accessor_type::get_it(group_, target_key);
				target_it == group_.end())
			{
				// not found, ignore it;
				return {group_.end(), group_.end()};
			}
			else
			{
				// try to find this key
				return {target_it, read_accessor_type::get_it(group_, key)};
			}
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert_before(const string_view_type target_key, const string_view_type key, string_type&& value) -> bool
		{
			if (const auto target_it = get_it(target_key, key).first;
				target_it != group_.end())
			{
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(target_it, target_it->first, group_type::mapped_type{key, std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert_before(string_view_type target_key, string_type&& key, string_type&& value) -> bool
		{
			if (const auto target_it = get_it(target_key, key).first;
				target_it != group_.end())
			{
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(target_it, target_it->first, group_type::mapped_type{std::move(key), std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert_after(const string_view_type target_key, const string_view_type key, string_type&& value) -> bool
		{
			if (auto target_it = get_it(target_key, key).first;
				target_it != group_.end())
			{
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(++target_it, target_it->first, group_type::mapped_type{key, std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::try_insert_after(string_view_type target_key, string_type&& key, string_type&& value) -> bool
		{
			if (auto target_it = get_it(target_key, key).first;
				target_it != group_.end())
			{
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(++target_it, target_it->first, group_type::mapped_type{std::move(key), std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign_before(string_view_type target_key, string_view_type key, string_type&& value) -> bool
		{
			if (const auto [target_it, it] = get_it(target_key, key);
				target_it != group_.end())
			{
				// already exists
				if (it != group_.end())
				{
					// extract it
					auto&& node			 = group_.extract(it);
					node.key()			 = target_it->first;
					node.mapped().second = std::move(value);
					// insert back
					return group_.insert(target_it, std::move(node)) != group_.end();
				}

				// emplace new one
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(target_it, target_it->first, group_type::mapped_type{key, std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign_before(string_view_type target_key, string_type&& key, string_type&& value) -> bool
		{
			if (const auto [target_it, it] = get_it(target_key, key);
				target_it != group_.end())
			{
				// already exists
				if (it != group_.end())
				{
					// extract it
					auto&& node			 = group_.extract(it);
					node.key()			 = target_it->first;
					node.mapped().second = std::move(value);
					// insert back
					return group_.insert(target_it, std::move(node)) != group_.end();
				}

				// emplace new one
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(target_it, target_it->first, group_type::mapped_type{std::move(key), std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign_after(string_view_type target_key, string_view_type key, string_type&& value) -> bool
		{
			if (auto [target_it, it] = get_it(target_key, key);
				target_it != group_.end())
			{
				// already exists
				if (it != group_.end())
				{
					// extract it
					auto&& node			 = group_.extract(it);
					node.key()			 = target_it->first;
					node.mapped().second = std::move(value);
					// insert back
					return group_.insert(target_it, std::move(node)) != group_.end();
				}

				// emplace new one
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(++target_it, target_it->first, group_type::mapped_type{key, std::move(value)}) != group_.end();
			}

			return false;
		}

		auto GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>::insert_or_assign_after(string_view_type target_key, string_type&& key, string_type&& value) -> bool
		{
			if (auto [target_it, it] = get_it(target_key, key);
				target_it != group_.end())
			{
				// already exists
				if (it != group_.end())
				{
					// extract it
					auto&& node			 = group_.extract(it);
					node.key()			 = target_it->first;
					node.mapped().second = std::move(value);
					// insert back
					return group_.insert(target_it, std::move(node)) != group_.end();
				}

				// emplace new one
				// insert it into the 'same' line of the target, but the new value 'insertion order' is specified before target.
				return group_.emplace_hint(++target_it, target_it->first, group_type::mapped_type{std::move(key), std::move(value)}) != group_.end();
			}

			return false;
		}
	}// namespace detail

	IniReader::IniReader(filename_view_type filename)
	{
		// todo: encoding?
		auto file = lexy::read_file<lexy::utf8_encoding>(filename.data());

		if (file)
		{
			UnorderedParseState<lexy::utf8_encoding> state{filename, file.buffer(), *this};
			lexy::parse<grammar::file<std::decay_t<decltype(state)>>>(file.buffer(), state, lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(filename.data()));
		}
	}
}// namespace gal::ini::impl
