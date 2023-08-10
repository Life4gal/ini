#include <ini/extractor.hpp>

#include <filesystem>
#include <ranges>
#include <algorithm>

#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>

#if defined(GAL_INI_COMPILER_MSVC)
#define GAL_INI_UNREACHABLE() __assume(0)
#elif defined(GAL_INI_COMPILER_GNU) || defined(GAL_INI_COMPILER_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_APPLE_CLANG)
#define GAL_INI_UNREACHABLE() __builtin_unreachable()
#else
	#define GAL_INI_UNREACHABLE() throw
#endif

// #define GAL_INI_DEBUG_TRACE

namespace
{
	namespace ini = gal::ini;

	using global_encoding = lexy::utf8_char_encoding;
	using global_char_type = global_encoding::char_type;
	using global_buffer_type = lexy::string_input<global_encoding>;
	using global_buffer_anchor_type = lexy::input_location_anchor<global_buffer_type>;
	using global_reader_type = decltype(std::declval<const global_buffer_type&>().reader());
	using global_position_type = global_reader_type::iterator;
	using global_lexeme_type = lexy::lexeme<global_reader_type>;

	class ErrorReporter
	{
	public:
		constexpr static ini::string_view_type buffer_filename{"anonymous-buffer"};

		static auto report_duplicate_declaration(
				const ini::string_view_type      identifier,
				const lexy_ext::diagnostic_kind  kind,
				const ini::string_view_type      category,
				const ini::string_view_type      what_to_do,
				const global_position_type       position,
				const global_buffer_type&        buffer,
				const global_buffer_anchor_type& anchor,
				const ini::string_view_type      filename,
				FILE*                            out_file = stderr
				) -> void
		{
			const auto location = lexy::get_input_location(buffer, position, anchor);

			const auto                        out = lexy::cfile_output_iterator{out_file};
			const lexy_ext::diagnostic_writer writer{buffer, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(
					out,
					kind,
					[&](lexy::cfile_output_iterator, lexy::visualization_options) -> lexy::cfile_output_iterator
					{
						(void)std::fprintf(out_file, "duplicate %s declaration named '%s', %s...", category.data(), identifier.data(), what_to_do.data());
						return out;
					});

			if (not filename.empty()) { (void)writer.write_path(out, filename.data()); }

			(void)writer.write_empty_annotation(out);
			(void)writer.write_annotation(
					out,
					lexy_ext::annotation_kind::primary,
					location,
					identifier.size(),
					[&](lexy::cfile_output_iterator, lexy::visualization_options) -> lexy::cfile_output_iterator
					{
						(void)std::fprintf(out_file, "second declaration here");
						return out;
					});
		}
	};

	namespace grammar
	{
		template<typename Return, typename... Functions>
		[[nodiscard]] constexpr auto callback(Functions&&... functions)
		{
			return lexy::bind(
					lexy::callback<Return>(std::forward<Functions>(functions)...),
					// out parse state
					lexy::parse_state,
					// parsed values
					lexy::values);
		}

		namespace dsl = lexy::dsl;

		struct string_literal
		{
			// A mapping of the simple escape sequences to their replacement values.
			// fixme: expected `template` keyword before dependent template name [-Werror=missing-template-keyword]
			#if defined(GAL_INI_COMPILER_GNU)
			#define WORKAROUND_GCC_TEMPLATE template
			#else
			#define WORKAROUND_GCC_TEMPLATE
			#endif
			constexpr static auto escaped_symbols = lexy::symbol_table<global_char_type>//
													.WORKAROUND_GCC_TEMPLATE map<'"'>('"')
													.WORKAROUND_GCC_TEMPLATE map<'\''>('\'')
													.WORKAROUND_GCC_TEMPLATE map<'\\'>('\\')
													.WORKAROUND_GCC_TEMPLATE map<'/'>('/')
													.WORKAROUND_GCC_TEMPLATE map<'b'>('\b')
													.WORKAROUND_GCC_TEMPLATE map<'f'>('\f')
													.WORKAROUND_GCC_TEMPLATE map<'n'>('\n')
													.WORKAROUND_GCC_TEMPLATE map<'r'>('\r')
													.WORKAROUND_GCC_TEMPLATE map<'t'>('\t');
			#undef WORKAROUND_GCC_TEMPLATE

			// Escape sequences start with a backlash and either map one of the symbols, or a Unicode code point.
			constexpr static auto escape = dsl::backslash_escape.symbol<escaped_symbols>().rule(
					// dsl::literal_set(dsl::lit_c<'u'>, dsl::lit_c<'U'>) >> dsl::code_point_id<4>
					dsl::lit_c<'u'> >> dsl::code_point_id<4> |
					dsl::lit_c<'U'> >> dsl::code_point_id<8>
					);
		};

		constexpr auto comment_indication = dsl::literal_set(dsl::lit_c<';'>, dsl::lit_c<'#'>);

		template<bool IsInline>
		struct comment
		{
			struct comment_context : lexy::transparent_production
			{
				constexpr static auto name = "[comment context]";

				constexpr static auto rule =
						dsl::identifier(
								// begin with printable
								dsl::unicode::print,
								// continue with printable, but excluding '\r', '\n' and '\r\n'
								// todo: multi-line comment
								dsl::unicode::print - dsl::unicode::newline);

				constexpr static auto value =
						[]
						{
							if constexpr (IsInline)
							{
								return lexy::callback<global_lexeme_type>(
										[](const global_lexeme_type lexeme) -> global_lexeme_type { return lexeme; });
							}
							else
							{
								return callback<void>(
										[](auto& state, const global_lexeme_type lexeme) -> void { state.comment(lexeme); });
							}
						}();
			};

			constexpr static auto name() noexcept -> const char*
			{
				if constexpr (IsInline) { return "[inline comment]"; }
				else { return "[comment]"; }
			}

			constexpr static auto rule = []
			{
				constexpr auto debug_begin = []
				{
					if constexpr (IsInline) { return LEXY_DEBUG("parse inline_comment begin"); }
					else { return LEXY_DEBUG("parse comment begin"); }
				}();

				constexpr auto debug_end = []
				{
					if constexpr (IsInline) { return LEXY_DEBUG("parse inline_comment end"); }
					else { return LEXY_DEBUG("parse comment end"); }
				}();

				return
						// begin with indication
						comment_indication >>
						(debug_begin +
						dsl::p<comment_context> +
						debug_end);
			}();

			constexpr static auto value = []
			{
				if constexpr (IsInline) { return lexy::forward<global_lexeme_type>; }
				else { return lexy::forward<void>; }
			}();
		};

		using comment_line = comment<false>;
		using comment_inline = comment<true>;

		template<typename State>
		struct property_name : lexy::transparent_production
		{
			struct invalid_name
			{
				constexpr static auto name = "a valid property name was required here";
			};

			using string_type = typename State::argument_type;
			constexpr static auto allocatable = State::allocatable;

			constexpr static auto name = []
			{
				if constexpr (allocatable) { return "[property name]"; }
				else { return "[property name (lexeme)]"; }
			}();

			constexpr static auto rule = []
			{
				if constexpr (allocatable)
				{
					return dsl::delimited(
							// begin with not '\r', '\n', '\r\n', whitespace or '='
							dsl::peek(dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign),
							// end until '='
							dsl::peek(dsl::equal_sign)
							)(dsl::unicode::print.error<invalid_name>, string_literal::escape);
				}
				else
				{
					// begin with not '\r', '\n', '\r\n', whitespace or '='
					constexpr auto begin = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

					// trailing with printable, but excluding '='
					constexpr auto trailing = dsl::unicode::print - dsl::equal_sign;

					return dsl::peek(begin) >>
							(LEXY_DEBUG("parse property key begin") +
							dsl::identifier(begin, trailing) +
							LEXY_DEBUG("parse property key end")) |
							// This error can make the line parsing fail immediately when the [key] cannot be parsed, and then skip this line (instead of trying to make other possible matches).
							dsl::error<invalid_name>;
				}
			}();

			constexpr static auto value = []
			{
				// fixme: string_type
				if constexpr (allocatable) { return lexy::as_string<std::remove_reference_t<string_type>, global_encoding>; }
				else { return lexy::forward<string_type>; }
			}();
		};

		template<typename State>
		struct property_value : lexy::transparent_production
		{
			using string_type = typename State::argument_type;
			constexpr static auto allocatable = State::allocatable;

			constexpr static auto name = []
			{
				if constexpr (allocatable) { return "[property value]"; }
				else { return "[property value (lexeme)]"; }
			}();

			constexpr static auto rule = []
			{
				if constexpr (allocatable)
				{
					return dsl::delimited(
							// begin with not '\r', '\n', '\r\n', whitespace, ';' or '#'
							dsl::peek(dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::lit_c<';'> - dsl::lit_c<'#'>),
							// comment_indication),
							// end until '\r', '\n', '\r\n', ';' or '#'
							dsl::peek(dsl::unicode::newline / dsl::lit_c<';'> / dsl::lit_c<'#'>))(dsl::unicode::print, string_literal::escape);
				}
				else
				{
					// begin with not '\r', '\n', '\r\n', whitespace, ';' or '#'
					constexpr auto begin = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::lit_c<';'> - dsl::lit_c<'#'>;// comment_indication

					// trailing with printable, but excluding '\r', '\n', '\r\n', ';' and '#'
					constexpr auto trailing = dsl::unicode::print - dsl::unicode::newline - dsl::lit_c<';'> - dsl::lit_c<'#'>;// comment_indication

					return dsl::peek(begin) >>
							(LEXY_DEBUG("parse property value begin") +
							dsl::identifier(begin, trailing) +
							LEXY_DEBUG("parse property value end"));
				}
			}();

			constexpr static auto value = []
			{
				// fixme: string_type
				if constexpr (allocatable) { return lexy::as_string<std::remove_reference_t<string_type>, global_encoding>; }
				else { return lexy::forward<string_type>; }
			}();
		};

		template<typename State>
		struct property : lexy::transparent_production
		{
			using string_type = typename State::argument_type;
			constexpr static auto allocatable = State::allocatable;

			constexpr static auto trim_right_whitespace = [](auto& key) -> void
			{
				// see property_name<String>::rule and property_value<String>::rule
				// Since whitespace is allowed to exist in the middle of a key string, we have to terminate with `=` / '\r' / '\n' / '\r\n'.
				// Since whitespace is allowed to exist in the middle of a value string, we have to terminate with  '\r' / '\n' / '\r\n' / ';' / '#'.
				// This means we have to manually remove the whitespace that follows on the right side.
				const auto view =
						key |
						std::views::reverse |
						std::views::take_while([](const auto c) -> bool { return std::isspace(c); });
				const auto                                   whitespace_count = std::ranges::distance(view);

				if (whitespace_count != 0)
				{
					if constexpr (allocatable)
					{
						// string
						key.resize(key.size() - whitespace_count);
					}
					else
					{
						// lexeme
						key = string_type{key.begin(), key.size() - whitespace_count};
					}
				}
			};

			constexpr static auto name = "[property]";

			constexpr static auto rule =
					LEXY_DEBUG("parse property begin") +
					dsl::position +
					dsl::p<property_name<State>> +
					dsl::equal_sign +
					dsl::opt(dsl::p<property_value<State>>) +
					dsl::opt(dsl::p<comment_inline>) +
					LEXY_DEBUG("parse property end");

			constexpr static auto value = []
			{
				return callback<void>(
						// key = value comment_inline
						[](
						auto&                      state,
						const global_position_type position,
						string_type                key,
						string_type                value,
						const global_lexeme_type   comment_inline) -> void
						{
							trim_right_whitespace(key);
							trim_right_whitespace(value);
							state.property(position, std::forward<string_type>(key), std::forward<string_type>(value), comment_inline);
						},
						// key = value
						[](
						auto&                      state,
						const global_position_type position,
						string_type                key,
						string_type                value,
						lexy::nullopt) -> void
						{
							trim_right_whitespace(key);
							trim_right_whitespace(value);
							state.property(position, std::forward<string_type>(key), std::forward<string_type>(value), {});
						},
						// key = comment_inline
						[](
						auto&                      state,
						const global_position_type position,
						string_type                key,
						lexy::nullopt,
						const global_lexeme_type comment_inline) -> void
						{
							trim_right_whitespace(key);
							state.property(position, std::forward<string_type>(key), {}, comment_inline);
						},
						// key =
						[](
						auto&                      state,
						const global_position_type position,
						string_type                key,
						lexy::nullopt,
						lexy::nullopt) -> void
						{
							trim_right_whitespace(key);
							state.property(position, std::forward<string_type>(key), {}, {});
						});
			}();
		};

		struct blank_line
		{
			constexpr static auto name = "[blank line]";

			constexpr static auto rule =
					dsl::peek(dsl::newline | dsl::unicode::blank) >>
					dsl::eol;

			constexpr static auto value = callback<void>(
					[](auto&      state) -> void { state.blank_line(); });
		};

		struct property_or_comment : lexy::scan_production<void>
		{
			constexpr static auto name = "[property or comment]";

			constexpr static auto rule =
					// blank line
					(dsl::p<blank_line>)
					|
					// comment
					(dsl::p<comment_line> >> dsl::eol)
					|
					// property
					(dsl::else_ >> (dsl::scan + dsl::eol));

			template<typename Context, typename Reader, typename State>
			constexpr static auto scan(lexy::rule_scanner<Context, Reader>& scanner, [[maybe_unused]] State& state) -> scan_result
			{
				scanner.parse(property<State>{});

				return {scanner.operator bool()};
			}
		};

		template<typename State>
		struct section_name
		{
			using string_type = typename State::argument_type;
			constexpr static auto allocatable = State::allocatable;

			constexpr static auto name = []
			{
				if constexpr (allocatable) { return "[section name]"; }
				else { return "[section name (lexeme)]"; }
			}();

			constexpr static auto rule = []
			{
				if constexpr (allocatable)
				{
					return dsl::delimited(
							// '['
							dsl::square_bracketed.open(),
							// ']'
							dsl::square_bracketed.close())(dsl::unicode::print, string_literal::escape);
				}
				else
				{
					return dsl::square_bracketed.open() +
							dsl::identifier(
									// begin with printable
									dsl::unicode::print,
									// continue with printable, but excluding ']'
									dsl::unicode::print - dsl::square_bracketed.close()) +
							dsl::square_bracketed.close();
				}
			}();

			constexpr static auto value = []
			{
				// fixme: string_type
				if constexpr (allocatable) { return lexy::as_string<std::remove_reference_t<string_type>, global_encoding>; }
				else { return lexy::forward<string_type>; }
			}();
		};

		struct section_declaration
		{
			struct header : lexy::scan_production<void>, lexy::transparent_production
			{
				constexpr static auto name = "[section header]";

				template<typename Context, typename Reader, typename State>
				constexpr static auto scan(lexy::rule_scanner<Context, Reader>& scanner, State& state) -> scan_result
				{
					const auto position = scanner.position();

					auto result = scanner.parse(section_name<State>{});
					if (not scanner) { return lexy::scan_failed; }

					if constexpr (State::allocatable) { state.section(position, std::move(result).value()); }
					else { state.section(position, result.value()); }

					return {true};
				}
			};

			constexpr static auto name = "[section declaration]";

			// end with 'eof' or next '[' (section begin)
			constexpr static auto rule =
					// fixme: currently only the comment in front of the first section is treated as belonging to that section, which means that the comment in other cases is treated as hitting [comment] when parsing [property or comment].
					dsl::if_(
							dsl::p<comment_line>
							// newline
							>>
							dsl::newline) +
					// [
					(
						LEXY_DEBUG("parse section_name begin") +
						dsl::p<header> +
						LEXY_DEBUG("parse section_name end") +
						dsl::newline +
						LEXY_DEBUG("parse section properties begin") +
						dsl::terminator(
								dsl::eof |
								dsl::peek(dsl::square_bracketed.open()))
						.opt_list(
								dsl::try_(
										dsl::p<property_or_comment>,
										// ignore this line if an error raised
										LEXY_DEBUG("ignore invalid line...") + dsl::until(dsl::newline))) +
						// strictly speaking, this is not 'just' the end of the previous section,
						// but also the possibility that 'variable_pair_or_comment' has already parsed the next section's comments.
						LEXY_DEBUG("parse section properties end"));

			constexpr static auto value = lexy::forward<void>;
		};

		struct file_context
		{
			constexpr static auto name = "[file content]";

			constexpr static auto whitespace = dsl::ascii::blank;

			constexpr static auto rule =
					dsl::terminator(dsl::eof)
					.opt_list(
							dsl::try_(
									dsl::p<section_declaration>,
									// ignore following lines until next section if an error raised
									dsl::until(dsl::square_bracketed.open()) + LEXY_DEBUG("ignore invalid section...")));

			constexpr static auto value = lexy::forward<void>;
		};
	}

	template<typename State, typename Buffer>
	auto parse(
			State&                      state,
			const Buffer&               buffer,
			const ini::string_view_type filename) -> void
	{
		#if defined(GAL_INI_DEBUG_TRACE)
		lexy::trace<grammar::file_context>(
				stderr,
				buffer,
				state,
				{.flags = lexy::visualize_fancy});

		state.trace_finish();
		#endif

		if (const auto result =
					lexy::parse<grammar::file_context>(
							buffer,
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(filename.data()));
			not result.has_value())
		{
			// todo: error ?
		}
	}

	template<typename State, typename Appender>
	auto do_check_and_extract_from_file(const ini::string_view_type filename, Appender appender) -> ini::ExtractResult
	{
		using ini::ExtractResult;

		if (std::error_code error_code = {};
			not std::filesystem::exists(filename.data(), error_code)) { return ExtractResult::FILE_NOT_FOUND; }

		if (const auto file = lexy::read_file<global_encoding>(filename.data());
			not file)
		{
			switch (file.error())
			{
				case lexy::file_error::file_not_found: { return ExtractResult::FILE_NOT_FOUND; }
				case lexy::file_error::permission_denied: { return ExtractResult::PERMISSION_DENIED; }
				case lexy::file_error::os_error: { return ExtractResult::INTERNAL_ERROR; }
				case lexy::file_error::_success:
				default: { GAL_INI_UNREACHABLE(); }
			}
		}
		else
		{
			const global_buffer_type buffer{file.buffer().data(), file.buffer().size()};

			State state{
					buffer,
					filename,
					appender};

			parse(state, buffer, filename);

			return ExtractResult::SUCCESS;
		}
	}

	template<typename State, typename Appender>
	auto do_check_and_extract_from_buffer(const ini::string_view_type buffer, Appender appender) -> ini::ExtractResult
	{
		using ini::ExtractResult;

		const global_buffer_type real_buffer{buffer.data(), buffer.size()};

		State state{
				real_buffer,
				ErrorReporter::buffer_filename,
				appender};

		parse(state, real_buffer, ErrorReporter::buffer_filename);

		return ExtractResult::SUCCESS;
	}

	template<typename String>
	class Extractor
	{
		using traits_type = ini::appender_traits<String>;
		using char_type = typename traits_type::char_type;

	public:
		constexpr static auto allocatable = traits_type::allocatable;

		using kv_appender_type = typename traits_type::kv_appender;
		using section_appender_type = typename traits_type::section_appender;

		using argument_type = std::conditional_t<allocatable, typename traits_type::argument_type, global_lexeme_type>;

	private:
		// never called
		using kv_argument_type = std::conditional_t<allocatable, typename traits_type::argument_type, std::basic_string_view<char_type>>;
		static auto error_kv_appender(kv_argument_type, kv_argument_type) -> typename traits_type::kv_append_result { throw std::runtime_error{"fixme"}; };

		auto redirect(const global_position_type position) noexcept -> void
		{
			const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			buffer_anchor_ = location.anchor();
		}

		auto do_append_section(argument_type section_name) -> typename traits_type::section_append_result
		{
			if constexpr (allocatable) { return section_appender_(std::forward<argument_type>(section_name)); }
			else
			{
				std::basic_string_view<char_type> name{section_name.data(), section_name.size()};
				return section_appender_(name);
			}
		}

		auto do_append_kv(argument_type key, argument_type value) -> typename traits_type::kv_append_result
		{
			if constexpr (allocatable) { return kv_appender_(std::forward<argument_type>(key), std::forward<argument_type>(value)); }
			else
			{
				std::basic_string_view<char_type> k{key.data(), key.size()};
				std::basic_string_view<char_type> v{value.data(), value.size()};

				return kv_appender_(k, v);
			}
		}

		const global_buffer_type& buffer_;
		global_buffer_anchor_type buffer_anchor_;
		ini::string_view_type     filename_;

		section_appender_type section_appender_;
		kv_appender_type      kv_appender_;

		#if defined(GAL_INI_DEBUG_TRACE)
		int debug_tracing_ = true;
		#endif

	public:
		Extractor(
				const global_buffer_type&   buffer,
				const ini::string_view_type filename,
				section_appender_type       section_appender)
			: buffer_{buffer},
			buffer_anchor_{buffer},
			filename_{filename},
			section_appender_{section_appender},
			kv_appender_{&error_kv_appender} {}

		// The parser ensures that if Extractor::comment is called, the indication must be valid.
		auto comment([[maybe_unused]] const global_lexeme_type context) const noexcept -> void
		{
			// todo: just ignore it?
			(void)this;
		}

		auto section(const global_position_type position, argument_type section_name) -> void
		{
			redirect(position);

			#if defined(GAL_INI_DEBUG_TRACE)

			if (debug_tracing_) { return; }

			#endif

			const auto [name, kv_appender, inserted] = this->do_append_section(std::forward<argument_type>(section_name));

			if (not inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						name,
						lexy_ext::diagnostic_kind::note,
						"section",
						"subsequent elements are appended to the previously declared section",
						position,
						buffer_,
						buffer_anchor_,
						filename_);
			}

			kv_appender_ = kv_appender;
		}

		auto property(const global_position_type position, argument_type key, argument_type value, [[maybe_unused]] const global_lexeme_type comment_inline) -> void
		{
			redirect(position);

			#if defined(GAL_INI_DEBUG_TRACE)

			if (debug_tracing_) { return; }

			#endif

			const auto [inserted_key, inserted_value, inserted] = this->do_append_kv(std::forward<argument_type>(key), std::forward<argument_type>(value));

			if (not inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						inserted_key,
						lexy_ext::diagnostic_kind::warning,
						"variable",
						"this variable will be discarded",
						position,
						buffer_,
						buffer_anchor_,
						filename_);
			}
		}

		auto blank_line() const noexcept -> void { (void)this; }

		#if defined(GAL_INI_DEBUG_TRACE)
		auto trace_finish() -> void
		{
			buffer_anchor_ = global_buffer_anchor_type{buffer_};
			debug_tracing_ = 0;
		}
		#endif
	};
}

namespace gal::ini
{
	auto extract_from_file(
			const string_view_type                                    filename,
			const appender_traits<string_view_type>::section_appender appender) -> ExtractResult { return do_check_and_extract_from_file<Extractor<string_view_type>>(filename, appender); }

	auto extract_from_file(
			const string_view_type                               filename,
			const appender_traits<string_type>::section_appender appender) -> ExtractResult { return do_check_and_extract_from_file<Extractor<string_type>>(filename, appender); }

	auto extract_from_buffer(
			const string_view_type                                    buffer,
			const appender_traits<string_view_type>::section_appender appender) -> ExtractResult { return do_check_and_extract_from_buffer<Extractor<string_view_type>>(buffer, appender); }

	auto extract_from_buffer(
			const string_view_type                               buffer,
			const appender_traits<string_type>::section_appender appender) -> ExtractResult { return do_check_and_extract_from_buffer<Extractor<string_type>>(buffer, appender); }
}
