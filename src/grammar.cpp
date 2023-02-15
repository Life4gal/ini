#include <filesystem>
#include <ini/internal/common.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>
#include <string_view>
#include <utility>

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
	#define CONSTEVAL constexpr
#else
	#define CONSTEVAL consteval
#endif

namespace
{
	template<typename StringType>
	[[nodiscard]] auto to_char_string(const StringType& string) -> decltype(auto)
	{
		// todo: char8_t/char16_t/char32_t
		if constexpr (std::is_same_v<typename StringType::value_type, char>) { return std::string_view{string.data(), string.size()}; }
		else { return std::filesystem::path{string.data(), string.data() + string.size()}.string(); }
	}
}// namespace

namespace gal::ini::grammar
{
	template<typename State>
	class ErrorReporter
	{
	public:
		constexpr static std::string_view buffer_file_path{"anonymous-buffer"};

		using state_type		 = State;
		using encoding			 = typename state_type::encoding;
		using string_type		 = typename state_type::string_type;
		using string_view_type	 = typename state_type::string_view_type;

		using buffer_type		 = lexy::string_input<encoding>;
		using buffer_anchor_type = lexy::input_location_anchor<buffer_type>;
		using reader_type		 = decltype(std::declval<const buffer_type&>().reader());
		using position_type		 = typename reader_type::iterator;
		using lexeme_type		 = lexy::lexeme<reader_type>;
		// using buffer_type		 = typename state_type::buffer_type;
		// using buffer_anchor_type = typename state_type::buffer_anchor_type;
		// using reader_type		 = typename state_type::reader_type;
		// using position_type		 = typename state_type::position_type;
		// using lexeme_type		 = typename state_type::lexeme_type;

		static auto report_duplicate_declaration(
				const string_type&				identifier,
				const lexy_ext::diagnostic_kind kind,
				const std::string_view			category,
				const std::string_view			what_to_do,
				const position_type				position,
				const buffer_type&				buffer,
				const buffer_anchor_type&		anchor,
				const std::string_view			file_path = buffer_file_path,
				FILE*							out_file  = stderr)
		{
			const auto						  location = lexy::get_input_location(buffer, position, anchor);

			const auto						  out	   = lexy::cfile_output_iterator{out_file};
			const lexy_ext::diagnostic_writer writer{buffer, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(
					out,
					kind,
					[&](lexy::cfile_output_iterator, lexy::visualization_options)
					{
						(void)std::fprintf(stderr, "duplicate %s declaration named '%s', %s...", category.data(), to_char_string(identifier).data(), what_to_do.data());
						return out;
					});

			if (!file_path.empty()) { (void)writer.write_path(out, file_path.data()); }

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
	};
}// namespace gal::ini::grammar

namespace
{
	using namespace gal::ini;

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

	template<bool Inline, typename State, typename State::char_type Indication>
	struct comment
	{
		constexpr static bool is_inline		  = Inline;

		using state_type					  = State;
		using char_type						  = typename state_type::char_type;

		using error_reporter_type			  = grammar::ErrorReporter<state_type>;

		constexpr static char_type indication = Indication;

		struct comment_context : lexy::transparent_production
		{
			[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[comment context]"; }

			constexpr static auto				rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n' and '\r\n'
							// todo: multi-line comment
							dsl::unicode::print - dsl::unicode::newline);

			// constexpr static auto value = callback<void>(
			// 		[](state_type& state, const typename error_reporter_type::lexeme_type lexeme) -> void
			// 		{
			// 			if constexpr (is_inline)
			// 			{
			// 				state.inline_comment(indication, typename error_reporter_type::string_view_type{lexeme.data(), lexeme.size()});
			// 			}
			// 			else
			// 			{
			// 				state.comment(indication, typename error_reporter_type::string_view_type{lexeme.data(), lexeme.size()});
			// 			}
			// 		});
			constexpr static auto value = lexy::callback<std::pair<char_type, typename error_reporter_type::lexeme_type>>(
					[](const typename error_reporter_type::lexeme_type lexeme) -> std::pair<char_type, typename error_reporter_type::lexeme_type>
					{
						return {indication, lexeme};
					});
		};

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[comment]"; }

		constexpr static auto				rule = []
		{
			constexpr auto debug_begin = []
			{
				if constexpr (is_inline)
				{
					return LEXY_DEBUG("parse inline_comment begin");
				}
				else
				{
					return LEXY_DEBUG("parse comment begin");
				}
			}();

			constexpr auto debug_end = []
			{
				if constexpr (is_inline)
				{
					return LEXY_DEBUG("parse inline_comment end");
				}
				else
				{
					return LEXY_DEBUG("parse comment end");
				}
			}();

			return
					// begin with indication
					dsl::lit_c<indication>
					//
					>>
					(
							// context begin
							debug_begin +
							dsl::p<comment_context> +
							// context end
							debug_end +
							// new line
							dsl::newline);
		};

		// constexpr static auto value = lexy::forward<void>;
		constexpr static auto value = lexy::forward<std::pair<char_type, typename error_reporter_type::lexeme_type>>;
	};

	template<typename State>
	using comment_hash_sign = comment<false, State, comment_indication_hash_sign<typename State::string_type>>;
	template<typename State>
	using comment_semicolon = comment<false, State, comment_indication_semicolon<typename State::string_type>>;

	template<typename State>
	constexpr auto comment_production =
			dsl::p<comment_hash_sign<State>> |
			dsl::p<comment_semicolon<State>>;

	template<typename State>
	using comment_inline_hash_sign = comment<true, State, comment_indication_hash_sign<typename State::string_view_type>>;
	template<typename State>
	using comment_inline_semicolon = comment<true, State, comment_indication_semicolon<typename State::string_view_type>>;

	template<typename State>
	constexpr auto comment_inline_production =
			dsl::p<comment_inline_hash_sign<State>> |
			dsl::p<comment_inline_semicolon<State>>;

	template<typename State>
	struct variable_key : lexy::transparent_production
	{
		using state_type		  = State;
		using char_type			  = typename state_type::char_type;

		using error_reporter_type = grammar::ErrorReporter<state_type>;

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[key]"; }

		struct invalid_key
		{
			constexpr static auto name = "a valid key required here";
		};

		constexpr static auto rule = []
		{
			// begin with not '\r', '\n', '\r\n', whitespace or '='
			constexpr auto begin_with_not_blank	   = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
			// continue with printable, but excluding '\r', '\n', '\r\n', whitespace and '='
			constexpr auto continue_with_printable = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

			return dsl::peek(begin_with_not_blank) >> dsl::identifier(begin_with_not_blank, continue_with_printable) |
				   // This error can make the line parsing fail immediately when the [key] cannot be parsed, and then skip this line (instead of trying to make other possible matches).
				   dsl::error<invalid_key>;
		}();

		constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
	};

	template<typename State>
	struct variable_value : lexy::transparent_production
	{
		using state_type		  = State;
		using char_type			  = typename state_type::char_type;

		using error_reporter_type = grammar::ErrorReporter<state_type>;

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[value]"; }

		constexpr static auto				rule = []
		{
			// begin with not '\r', '\n', '\r\n', whitespace or '='
			constexpr auto begin_with_not_blank =
					dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign
					// todo: we need a better way to support the possible addition of comment formats in the future.
					// see also: variable_or_comment::rule -> dsl::peek(...)
					- dsl::lit_c<comment_hash_sign<State>::indication> - dsl::lit_c<comment_semicolon<State>::indication>;

			// continue with printable, but excluding '\r', '\n', '\r\n', whitespace and '='
			constexpr auto continue_with_printable = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

			return dsl::peek(begin_with_not_blank) >> dsl::identifier(begin_with_not_blank, continue_with_printable);
		}();

		constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
	};

	// identifier = [variable]
	template<typename State>
	struct variable_declaration : lexy::transparent_production
	{
		using state_type		  = State;
		using char_type			  = typename state_type::char_type;

		using error_reporter_type = grammar::ErrorReporter<state_type>;

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

		constexpr static auto				rule =
				LEXY_DEBUG("parse variable_declaration begin") +
				dsl::position +
				dsl::p<variable_key<State>> +
				dsl::equal_sign +
				dsl::opt(dsl::p<variable_value<State>>) +
				dsl::opt(comment_inline_production<State>) +
				LEXY_DEBUG("parse variable_declaration end");

		constexpr static auto value = callback<void>(
				// blank line
				// []([[maybe_unused]] State& state) {},
				// [identifier] = [variable] [inline_comment]
				[](
						state_type&															  state,
						const typename error_reporter_type::position_type					  position,
						const typename error_reporter_type::lexeme_type						  variable_key,
						const typename error_reporter_type::lexeme_type						  variable_value,
						const std::pair<char_type, typename error_reporter_type::lexeme_type> inline_comment) -> void
				{
					state.value(
							position,
							variable_key,
							variable_value,
							inline_comment);
				},
				// [identifier] = [] [inline_comment]
				[](
						state_type&										  state,
						const typename error_reporter_type::position_type position,
						const typename error_reporter_type::lexeme_type	  variable_key,
						lexy::nullopt,
						const std::pair<char_type, typename error_reporter_type::lexeme_type> inline_comment) -> void
				{
					state.value(
							position,
							variable_key,
							{},
							inline_comment);
				},
				// [identifier] = [variable] []
				[](
						state_type&										  state,
						const typename error_reporter_type::position_type position,
						const typename error_reporter_type::lexeme_type	  variable_key,
						const typename error_reporter_type::lexeme_type	  variable_value,
						lexy::nullopt)
				{
					state.value(
							position,
							variable_key,
							variable_value,
							{});
				});
	};

	template<typename State>
	struct variable_or_comment
	{
		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[variable or comment]"; }

		constexpr static auto				rule =
				// ignore blank line
				(dsl::peek(dsl::newline | dsl::unicode::blank) >>
				 (LEXY_DEBUG("ignore empty line") +
				  dsl::until(dsl::newline).or_eof())) |
				// comment
				// todo: sign?
				(dsl::peek(
						 dsl::lit_c<comment_hash_sign<State>::indication> |
						 dsl::lit_c<comment_semicolon<State>::indication>) >>
				 comment_production<State>) |
				// variable
				(dsl::else_ >>
				 (dsl::p<variable_declaration<State>> +
				  // a newline required
				  dsl::newline));

		constexpr static auto value = lexy::forward<void>;
	};

	template<typename State>
	struct group_name : lexy::transparent_production
	{
		using state_type		  = State;
		using char_type			  = typename state_type::char_type;

		using error_reporter_type = grammar::ErrorReporter<state_type>;

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[group name]"; }

		constexpr static auto				rule =
				dsl::identifier(
						// begin with printable
						dsl::unicode::print,
						// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
						dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

		constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
	};

	template<typename State>
	struct group_declaration
	{
		struct header : lexy::transparent_production
		{
			using state_type		  = State;
			using char_type			  = typename state_type::char_type;

			using error_reporter_type = grammar::ErrorReporter<state_type>;

			[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[group head]"; }

			constexpr static auto				rule =
					LEXY_DEBUG("parse group_name begin") +
					dsl::position +
					// group name
					dsl::p<group_name<State>> +
					LEXY_DEBUG("parse group_name end") +
					// ]
					dsl::square_bracketed.close() +
					dsl::opt(comment_inline_production<State>) +
					dsl::until(dsl::newline);

			constexpr static auto value = callback<void>(
					// [group_name] [inline_comment]
					[](
							state_type&															  state,
							const typename error_reporter_type::position_type					  position,
							typename error_reporter_type::lexeme_type							  group_name,
							const std::pair<char_type, typename error_reporter_type::lexeme_type> inline_comment) -> void
					{
						state.group(
								position,
								group_name,
								inline_comment);
					},
					// [group_name] []
					[](
							state_type&										  state,
							const typename error_reporter_type::position_type position,
							typename error_reporter_type::lexeme_type		  group_name,
							lexy::nullopt) -> void
					{
						state.group(
								position,
								group_name,
								{});
					});
		};

		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[group declaration]"; }

		// end with 'eof' or next '[' (group begin)
		constexpr static auto				rule =
				dsl::if_(comment_production<State>) +
				// [
				(dsl::square_bracketed.open() >>
				 (dsl::p<header> +
				  LEXY_DEBUG("parse group properties begin") +
				  dsl::terminator(
						  dsl::eof |
						  dsl::peek(dsl::square_bracketed.open()))
						  .opt_list(
								  dsl::try_(
										  dsl::p<variable_or_comment<State>>,
										  // ignore this line if an error raised
										  dsl::until(dsl::newline))) +
				  LEXY_DEBUG("parse group properties end")));

		constexpr static auto value = lexy::forward<void>;
	};

	template<typename State>
	struct context
	{
		[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[context]"; }

		constexpr static auto				whitespace = dsl::ascii::blank;

		constexpr static auto				rule =
				dsl::terminator(dsl::eof)
						.opt_list(
								dsl::try_(
										dsl::p<group_declaration<State>>,
										// ignore this line if an error raised
										dsl::until(dsl::newline)));

		constexpr static auto value = lexy::forward<void>;
	};
}// namespace

namespace gal::ini::grammar
{
	template<typename State>
	auto parse(
			State&											  state,
			const typename ErrorReporter<State>::buffer_type& buffer,
			std::string_view								  file_path) -> void
	{
		if (const auto result =
					lexy::parse<context<State>>(
							buffer(),
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_path.data()));
			!result.has_value())
		{
			// todo: error ?
		}
	}

	template<typename State>
	auto parse(
			State&											  state,
			const typename ErrorReporter<State>::buffer_type& buffer,
			std::string_view								  file_path,
			FILE*											  out_file// = stderr
			) -> void
	{
		parse(state, buffer, file_path);

		lexy::trace<context<State>>(
				out_file,
				buffer,
				{.flags = lexy::visualize_fancy});
	}
}// namespace gal::ini::grammar
