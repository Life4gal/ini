#include <fstream>
#include <ini/ini.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>

namespace gal::ini
{
	class ParseState
	{
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

		using context_type = lexy::buffer<lexy::utf8_encoding>;
		using char_type	   = context_type::char_type;

	private:
		context_type							  buffer_;
		lexy::input_location_anchor<context_type> buffer_anchor_;

		string_view_type						  filename_;
		Ini&									  ini_;

		auto									  report_duplicate_declaration(const char_type* position, const string_view_type identifier, const string_view_type category) const -> void
		{
			const auto						  location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			const auto						  out	   = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{buffer_, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(out,
									   lexy_ext::diagnostic_kind::error,
									   [&](lexy::cfile_output_iterator, lexy::visualization_options)
									   {
										   (void)std::fprintf(stderr, "duplicate %s declaration named '%s'", category.data(), identifier.data());
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

		auto debug_print_variable(const char_type* position, const string_view_type key, const string_view_type value) const -> void
		{
			const auto						  location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			const auto						  out	   = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{buffer_, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(out,
									   lexy_ext::diagnostic_kind::info,
									   [&](lexy::cfile_output_iterator, lexy::visualization_options)
									   {
										   (void)std::fprintf(stderr, "[%s]: %s = %s", ini_.current_group_->first.c_str(), key.data(), value.data());
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
				const string_view_type filename,
				context_type&&		   buffer,
				Ini&				   ini)
			: buffer_{std::move(buffer)},
			  buffer_anchor_{this->buffer_},
			  filename_{filename},
			  ini_{ini} {}

		// ParseState(const ParseState&)	  = delete;
		// ParseState(ParseState&&)		  = delete;
		// auto operator=(const ParseState&) = delete;
		// auto operator=(ParseState&&)	  = delete;
		//
		// ~ParseState() noexcept
		// {
		// 	// reset group iterator
		// 	ini_.current_group_ = ini_.context_.end();
		// }

		auto begin_parse() -> void;

		auto end_parse() const -> void
		{
			// reset group iterator
			ini_.current_group_ = ini_.context_.end();
		}

		auto begin_group(const char_type* position, string_type&& group_name) -> void
		{
			auto [it, inserted] = ini_.context_.try_emplace(std::move(group_name), Ini::group_type{});
			if (!inserted) { report_duplicate_declaration(position, it->first, "group"); }

			ini_.current_group_ = it;
		}

		auto value(const char_type* position, string_type&& key, string_type&& value) -> void
		{
			// Our parse ensures the current_group is valid
			auto& [_, group]	= *ini_.current_group_;
			auto [it, inserted] = group.try_emplace(std::move(key), std::move(value));
			if (!inserted) { report_duplicate_declaration(position, it->first, "variable"); }

			// debug_print_variable(position, it->first, "variable");
		}
	};
}// namespace gal::ini

namespace
{
	namespace ini = gal::ini;

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		struct identifier
		{
			constexpr static auto rule =
					dsl::identifier(
							// begin with alpha/digit/underscore
							dsl::ascii::alpha_digit_underscore,
							// continue with alpha/digit/underscore
							dsl::ascii::alpha_digit_underscore);

			constexpr static auto value = lexy::as_string<ini::string_type, ini::ParseState::context_type::encoding>;
		};

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

			constexpr static auto value = lexy::as_string<ini::string_type, ini::ParseState::context_type::encoding>;
		};

		// identifier = [variable]
		struct variable_declaration
		{
			constexpr static auto rule =
					dsl::position +
					dsl::p<identifier> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable>);

			constexpr static auto value = ini::ParseState::callback<void>(
					// identifier = variable
					[](ini::ParseState& state, const ini::ParseState::char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
					{ state.value(position, std::move(key), std::move(value)); },
					// identifier =
					[](ini::ParseState& state, const ini::ParseState::char_type* position, ini::string_type&& key, lexy::nullopt) -> void
					{ state.value(position, std::move(key), ini::string_type{}); });
		};

		// [group_name]
		struct group_declaration
		{
			struct header : lexy::transparent_production
			{
				constexpr static auto rule =
						// [
						dsl::square_bracketed.open() >>
						(dsl::position +
						 // group name
						 dsl::p<identifier> +
						 // ]
						 dsl::square_bracketed.close());

				constexpr static auto value = ini::ParseState::callback<void>(
						[](ini::ParseState& state, const ini::ParseState::char_type* position, ini::string_type&& group_name) -> void
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
					dsl::terminator(dsl::eof | dsl::peek(dsl::square_bracketed.open())).opt_list(dsl::p<variable_declaration>);

			constexpr static auto value = lexy::forward<void>;
		};

		struct file
		{
			constexpr static auto whitespace =
					// space
					dsl::ascii::blank |
					// The newline character is treated as a whitespace here, allowing us to skip the newline character, but this also leads to our above branching rules can no longer rely on the newline character.
					dsl::unicode::newline |
					// comment
					dsl::hash_sign >> dsl::until(dsl::newline);

			constexpr static auto rule	= dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration>);

			constexpr static auto value = lexy::forward<void>;
		};
	}// namespace grammar
}// namespace

namespace gal::ini
{
	auto ParseState::begin_parse() -> void { lexy::parse<grammar::file>(buffer_, *this, lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(filename_.data())); }

	Ini::Ini(string_type&& filename)
		: filename_{std::move(filename)}
	{
		auto file = lexy::read_file<lexy::utf8_encoding>(filename_.c_str());

		if (!file)
		{
			// parse failed
			return;
		}

		ParseState state{filename_, std::move(file).buffer(), *this};
		state.begin_parse();
		state.end_parse();
	}

	Ini::~Ini() noexcept { flush(); }

	auto Ini::flush() const -> void
	{
		std::ofstream file{filename_.c_str(), std::ios::out | std::ios::trunc};

		if (!file.is_open())
		{
			// todo
			return;
		}

		print(file);

		file.close();
	}
}// namespace gal::ini
