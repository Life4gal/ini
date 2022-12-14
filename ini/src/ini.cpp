#include <filesystem>
#include <fstream>
#include <ini/impl/ini_v2.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>

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

	using group_type   = ini::group_type;
	using context_type = ini::context_type;

	template<typename Encoding>
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

		using encoding	  = Encoding;
		using buffer_type = lexy::buffer<encoding>;
		using char_type	  = buffer_type::char_type;

		static_assert(std::is_same_v<encoding, typename buffer_type::encoding>);

	private:
		buffer_type								 buffer_;
		lexy::input_location_anchor<buffer_type> buffer_anchor_;

		ini::filename_view_type					 filename_;

		context_type							 context_;
		context_type::iterator					 current_group_;

	private:
		auto report_duplicate_declaration(const char_type* position, const ini::string_view_type identifier, const ini::string_view_type category) const -> void
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
										   (void)std::fprintf(stderr, "[%s]: %s = %s", to_char_string(current_group_->first).data(), to_char_string(key).data(), to_char_string(value).data());
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
				buffer_type&&				  buffer)
			: buffer_{std::move(buffer)},
			  buffer_anchor_{this->buffer_},
			  filename_{filename},
			  context_{},
			  current_group_{context_.end()} {}

		auto buffer() & -> buffer_type&
		{
			return buffer_;
		}

		auto context() & -> context_type&
		{
			return context_;
		}

		auto context() && -> context_type&&
		{
			return std::move(context_);
		}

		auto begin_group(const char_type* position, ini::string_type&& group_name) -> void
		{
			// [[maybe_unused]] const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			auto [it, inserted] = context_.try_emplace(std::move(group_name), group_type{});
			if (!inserted) { report_duplicate_declaration(position, it->first, "group"); }

			current_group_ = it;
		}

		auto value(const char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
		{
			// [[maybe_unused]] const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			// Our parse ensures the current_group is valid
			auto& [_, group]	= *current_group_;
			auto [it, inserted] = group.try_emplace(std::move(key), std::move(value));
			if (!inserted) { report_duplicate_declaration(position, it->first, "variable"); }

			// debug_print_variable(position, it->first, "variable");
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

	auto read(ini::filename_view_type filename) -> context_type
	{
		// todo: encoding?
		auto file = lexy::read_file<lexy::utf8_encoding>(filename.data());

		if (file)
		{
			ParseState state{filename, std::move(file).buffer()};
			lexy::parse<grammar::file<std::decay_t<decltype(state)>>>(state.buffer(), state, lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(filename.data()));
			return std::move(state).context();
		}

		// todo
		return {};
	}
}// namespace

namespace gal::ini::impl::inline v2
{
	Ini::Ini(filename_type&& filename)
		: filename_{std::move(filename)},
		  context_{read(filename_)},
		  current_group_{context_.end()}
	{
	}

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
}// namespace gal::ini::impl::inline v2
