#include <cassert>
#include <filesystem>
#include <fstream>
#include <ini/extractor.hpp>
#include <ini/flusher.hpp>
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

//#define GAL_INI_DEBUG_GROUP
//#define GAL_INI_DEBUG_TRACE

namespace
{
	namespace ini = gal::ini;

	template<typename State>
	class ErrorReporter
	{
		template<typename StringType>
		[[nodiscard]] static auto to_char_string(const StringType& string) -> decltype(auto)
		{
			// todo: char8_t/char16_t/char32_t
			if constexpr (std::is_same_v<typename StringType::value_type, char>) { return std::string_view{string.data(), string.size()}; }
			else { return std::filesystem::path{string.data(), string.data() + string.size()}.string(); }
		}

	public:
		using state_type		 = State;
		using encoding			 = typename state_type::encoding;

		using char_type			 = typename encoding::char_type;
		using identifier_type	 = std::basic_string_view<char_type>;// typename state_type::identifier_type;

		using buffer_type		 = lexy::string_input<encoding>;
		using buffer_anchor_type = lexy::input_location_anchor<buffer_type>;
		using reader_type		 = decltype(std::declval<const buffer_type&>().reader());
		using position_type		 = typename reader_type::iterator;
		using lexeme_type		 = lexy::lexeme<reader_type>;

		constexpr static std::string_view buffer_file_path{"anonymous-buffer"};

		static auto						  report_duplicate_declaration(
									  const identifier_type			  identifier,
									  const lexy_ext::diagnostic_kind kind,
									  const std::string_view		  category,
									  const std::string_view		  what_to_do,
									  const position_type			  position,
									  const buffer_type&			  buffer,
									  const buffer_anchor_type&		  anchor,
									  const std::string_view		  file_path,
									  FILE*							  out_file = stderr)
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

		template<bool Inline, typename State, typename State::char_type Indication>
		struct comment
		{
			constexpr static bool is_inline		  = Inline;

			using state_type					  = State;
			using error_reporter_type			  = ErrorReporter<state_type>;
			using char_type						  = typename error_reporter_type::char_type;

			constexpr static char_type indication = Indication;

			struct comment_context : lexy::transparent_production
			{
				[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char*
				{
					return "[comment context]";
				}

				constexpr static auto rule =
						dsl::identifier(
								// begin with printable
								dsl::unicode::print,
								// continue with printable, but excluding '\r', '\n' and '\r\n'
								// todo: multi-line comment
								dsl::unicode::print - dsl::unicode::newline);

				constexpr static auto value = []
				{
					if constexpr (is_inline)
					{
						return lexy::callback<std::pair<char_type, typename error_reporter_type::lexeme_type>>(
								[](const typename error_reporter_type::lexeme_type lexeme) -> std::pair<char_type, typename error_reporter_type::lexeme_type>
								{
									return {indication, lexeme};
								});
					}
					else
					{
						return callback<void>(
								[](state_type& state, const typename error_reporter_type::lexeme_type lexeme) -> void
								{
									state.comment(indication, lexeme);
								});
					}
				}();
			};

			[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char*
			{
				// todo
				constexpr auto type = ini::make_comment_indication<typename error_reporter_type::identifier_type>(indication);
				static_assert(type != ini::CommentIndication::INVALID);

#define comment_name_gen(comment_type)                            \
	if constexpr (type == ini::CommentIndication::HASH_SIGN)      \
	{                                                             \
		return "[" #comment_type " --> `#`]";                     \
	}                                                             \
	else if constexpr (type == ini::CommentIndication::SEMICOLON) \
	{                                                             \
		return "[" #comment_type " --> `;`]";                     \
	}                                                             \
	else                                                          \
	{                                                             \
		GAL_INI_UNREACHABLE();                                    \
	}

				if constexpr (is_inline)
				{
					comment_name_gen(inline_comment);
				}
				else
				{
					comment_name_gen(comment);
				}

#undef comment_name_gen
			}

			constexpr static auto rule = []
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
								debug_end
								// note: comment `does not consume` the newline
						);
			}();

			constexpr static auto value = []
			{
				if constexpr (is_inline)
				{
					return lexy::forward<std::pair<char_type, typename error_reporter_type::lexeme_type>>;
				}
				else
				{
					return lexy::forward<void>;
				}
			}();
		};

		template<typename State>
		using comment_hash_sign = comment<false, State, ini::comment_indication_hash_sign<typename State::identifier_type>>;
		template<typename State>
		using comment_semicolon = comment<false, State, ini::comment_indication_semicolon<typename State::identifier_type>>;

		template<typename State>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<State>> |
				dsl::p<comment_semicolon<State>>;

		template<typename State>
		using comment_inline_hash_sign = comment<true, State, ini::comment_indication_hash_sign<typename State::string_view_type>>;
		template<typename State>
		using comment_inline_semicolon = comment<true, State, ini::comment_indication_semicolon<typename State::string_view_type>>;

		template<typename State>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<State>> |
				dsl::p<comment_inline_semicolon<State>>;

		template<typename State>
		struct variable_key : lexy::transparent_production
		{
			using state_type		  = State;
			using error_reporter_type = ErrorReporter<state_type>;
			using char_type			  = typename error_reporter_type::char_type;

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

				return dsl::peek(begin_with_not_blank) >>
							   (LEXY_DEBUG("parse variable key begin") +
								dsl::identifier(begin_with_not_blank, continue_with_printable) +
								LEXY_DEBUG("parse variable key end")) |
					   // This error can make the line parsing fail immediately when the [key] cannot be parsed, and then skip this line (instead of trying to make other possible matches).
					   dsl::error<invalid_key>;
			}();

			constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
		};

		template<typename State>
		struct variable_value : lexy::transparent_production
		{
			using state_type		  = State;
			using error_reporter_type = ErrorReporter<state_type>;
			using char_type			  = typename error_reporter_type::char_type;

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

				return dsl::peek(begin_with_not_blank) >>
					   (LEXY_DEBUG("parse variable value begin") +
						dsl::identifier(begin_with_not_blank, continue_with_printable) +
						LEXY_DEBUG("parse variable value end"));
			}();

			constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
		};

		// identifier = [variable]
		template<typename State>
		struct variable_declaration : lexy::transparent_production
		{
			using state_type		  = State;
			using error_reporter_type = ErrorReporter<state_type>;
			using char_type			  = typename error_reporter_type::char_type;

			[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[variable pair declaration]"; }

			constexpr static auto				rule =
					LEXY_DEBUG("parse variable_declaration begin") +
					dsl::position +
					dsl::p<variable_key<State>> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable_value<State>>) +
					dsl::opt(comment_inline_production<State>) +
					LEXY_DEBUG("parse variable_declaration end")
					// note: variable_declaration `does not consume` the newline
					;

			constexpr static auto value = callback<void>(
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
							lexy::nullopt) -> void
					{
						state.value(
								position,
								variable_key,
								variable_value,
								{});
					},
					// [identifier] = [] []
					[](
							state_type&										  state,
							const typename error_reporter_type::position_type position,
							const typename error_reporter_type::lexeme_type	  variable_key,
							lexy::nullopt,
							lexy::nullopt) -> void
					{
						state.value(
								position,
								variable_key,
								{},
								{});
					});
		};

		template<typename State>
		struct blank_line
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[empty line]"; }

			constexpr static auto				rule =
					// dsl::peek(dsl::newline | dsl::unicode::blank) >> dsl::until(dsl::newline).or_eof();
					dsl::newline;

			constexpr static auto value = callback<void>(
					[](State& state) -> void
					{ state.blank_line(); });
		};

		template<typename State>
		struct variable_or_comment
		{
			[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[variable or comment]"; }

			constexpr static auto				rule =
					(dsl::peek(dsl::newline | dsl::unicode::blank) >> dsl::p<blank_line<State>>) |
					// comment
					// todo: sign?
					(dsl::peek(
							 dsl::lit_c<comment_hash_sign<State>::indication> |
							 dsl::lit_c<comment_semicolon<State>::indication>) >>
					 (comment_production<State> +
					  // newline
					  dsl::newline)) |
					// variable
					(dsl::else_ >>
					 (dsl::p<variable_declaration<State>> +
					  // newline
					  dsl::newline));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename State>
		struct group_declaration
		{
			struct header : lexy::transparent_production
			{
				using state_type		  = State;
				using error_reporter_type = ErrorReporter<state_type>;
				using char_type			  = typename error_reporter_type::char_type;

				struct group_name : lexy::transparent_production
				{
					[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[group name]"; }

					constexpr static auto				rule =
							dsl::identifier(
									// begin with printable
									dsl::unicode::print,
									// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
									dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

					constexpr static auto value = lexy::forward<typename error_reporter_type::lexeme_type>;
				};

				[[nodiscard]] CONSTEVAL static auto name() noexcept -> const char* { return "[group head]"; }

				constexpr static auto				rule =
						dsl::position +
						// group name
						LEXY_DEBUG("parse group_name begin") +
						dsl::p<group_name> +
						// ]
						dsl::square_bracketed.close() +
						LEXY_DEBUG("parse group_name end") +
						dsl::opt(comment_inline_production<State>) +
						// newline
						dsl::newline;

				constexpr static auto value = callback<void>(
						// [group_name] [inline_comment]
						[](
								state_type&															  state,
								const typename error_reporter_type::position_type					  position,
								const typename error_reporter_type::lexeme_type						  group_name,
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
								const typename error_reporter_type::lexeme_type	  group_name,
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
					dsl::if_(
							comment_production<State>
							// newline
							>>
							dsl::newline) +
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
											  LEXY_DEBUG("ignore invalid line...") + dsl::until(dsl::newline))) +
					  // strictly speaking, this is not 'just' the end of the previous group,
					  // but also the possibility that 'variable_or_comment' has already parsed the next group's comments.
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
											// ignore following lines until next group if an error raised
											dsl::until(dsl::square_bracketed.open()) + LEXY_DEBUG("ignore invalid group...")));

			constexpr static auto value = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename State>
	auto parse(
			State&											 state,
			const typename ErrorReporter<State>::buffer_type buffer,
			std::string_view								 file_path) -> void
	{
#if defined(GAL_INI_DEBUG_TRACE)
		lexy::trace<grammar::context<State>>(
				stderr,
				buffer,
				{.flags = lexy::visualize_fancy});
#endif

		if (const auto result =
					lexy::parse<grammar::context<State>>(
							buffer,
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_path.data()));
			!result.has_value())
		{
			// todo: error ?
		}
	}

	// ========================================
	// EXTRACTOR
	// ========================================

	template<typename Encoding, typename GroupAppend, typename KvAppend>
	class Extractor
	{
	public:
		using encoding			  = Encoding;
		using group_append_type	  = GroupAppend;
		using kv_append_type	  = KvAppend;

		using char_type			  = typename Encoding::char_type;
		using string_view_type	  = std::basic_string_view<char_type>;

		using identifier_type	  = decltype(std::declval<group_append_type>()(std::declval<string_view_type>()).name);

		using error_reporter_type = ErrorReporter<std::remove_cvref_t<Extractor>>;

		using buffer_type		  = typename error_reporter_type::buffer_type;
		using buffer_anchor_type  = typename error_reporter_type::buffer_anchor_type;
		using reader_type		  = typename error_reporter_type::reader_type;
		using position_type		  = typename error_reporter_type::position_type;
		using lexeme_type		  = typename error_reporter_type::lexeme_type;

	private:
		const buffer_type& buffer_;
		buffer_anchor_type buffer_anchor_;
		std::string_view   file_path_;

		group_append_type  group_appender_;
		kv_append_type	   kv_appender_;

#if defined(GAL_INI_DEBUG_GROUP)
		string_view_type current_group_;
#endif

	public:
		Extractor(
				const buffer_type&	   buffer,
				const std::string_view file_path,
				group_append_type	   group_appender)
			: buffer_{buffer},
			  buffer_anchor_{buffer_},
			  file_path_{file_path},
			  group_appender_{group_appender},
			  kv_appender_{}
		{
		}

		// The parser ensures that if Extractor::comment is called, the indication must be valid.
		auto comment(
				[[maybe_unused]] const char_type   indication,
				[[maybe_unused]] const lexeme_type context) -> void
		{
			// todo: just ignore it?
		}

		// The parser will pass an empty inline_comment if it does not resolve it (unlike Extractor::comment).
		auto group(
				const position_type										 position,
				const lexeme_type										 group_name,
				[[maybe_unused]] const std::pair<char_type, lexeme_type> inline_comment) -> void
		{
			// todo: ignore inline_comment?

			const string_view_type user_group_name{group_name.data(), group_name.size()};
			const auto [name, kv_appender, inserted] = group_appender_(user_group_name);

#if defined(GAL_INI_DEBUG_GROUP)
			current_group_ = name;
#endif

			if (!inserted)
			{
				error_reporter_type::report_duplicate_declaration(
						name,
						lexy_ext::diagnostic_kind::note,
						"group",
						"subsequent elements are appended to the previously declared group",
						position,
						buffer_,
						buffer_anchor_,
						file_path_);
			}

			kv_appender_ = kv_appender;
		}

		auto value(
				const position_type										 position,
				const lexeme_type										 variable_key,
				const lexeme_type										 variable_value,
				[[maybe_unused]] const std::pair<char_type, lexeme_type> inline_comment) -> void
		{
			// todo: ignore inline_comment?

			const string_view_type user_key{variable_key.data(), variable_key.size()};
			const string_view_type user_value{variable_value.data(), variable_value.size()};

			// Our parse ensures the kv_appender_ is valid
			if (
					const auto& [kv, inserted] = kv_appender_(user_key, user_value);
					!inserted)
			{
				error_reporter_type::report_duplicate_declaration(
						kv.first,
						lexy_ext::diagnostic_kind::warning,
						"variable",
						"this variable will be discarded",
						position,
						buffer_,
						buffer_anchor_,
						file_path_);
			}
		}

		auto blank_line() -> void
		{
		}
	};

	// ========================================
	// FLUSHER
	// ========================================

	template<bool, typename...>
	class FlushFile;

	template<typename Char>
	class FlushFile<false, Char>
	{
	public:
		using path_type = std::filesystem::path;
		using out_type	= std::basic_ofstream<Char>;

	private:
		path_type source_path_;
		path_type temp_path_;
		out_type  out_;

	public:
		explicit FlushFile(const std::string_view file_path)
			: source_path_{file_path},
			  temp_path_{
					  std::filesystem::temp_directory_path() /
					  // todo: Do we need a unique file name? Because if we open the file in std::ios::trunc mode and write less data than before, some unwanted garbage data will be left in the file.
					  source_path_.stem().string().append(std::to_string(reinterpret_cast<std::uintptr_t>(&source_path_))).append(source_path_.extension().string())
					  // source_path_.filename()
			  },
			  out_{temp_path_, std::ios::out | std::ios::trunc}
		{
			// todo: test?
			assert(ready() && "Cannot open file!");
		}

		FlushFile(const FlushFile&)					   = delete;
		FlushFile(FlushFile&&)						   = delete;
		auto operator=(const FlushFile&) -> FlushFile& = delete;
		auto operator=(FlushFile&&) -> FlushFile&	   = delete;

		~FlushFile() noexcept
		{
			out_.close();

			std::filesystem::copy_file(
					temp_path_,
					source_path_,
					std::filesystem::copy_options::overwrite_existing);
		}

		[[nodiscard]] auto ready() const noexcept -> bool { return exists(temp_path_) && out_.is_open() && out_.good(); }

		template<typename Data>
			requires requires {
						 out_ << std::declval<Data>();
					 }
		auto operator<<(const Data& data) -> FlushFile&
		{
			out_ << data;
			return *this;
		}

		[[nodiscard]] auto out() noexcept -> out_type&
		{
			return out_;
		}
	};

	template<typename... Args>
	class FlushFile<true, Args...>
	{
	public:
		explicit FlushFile([[maybe_unused]] const std::string_view file_path) {}
	};

	template<typename Encoding, typename GroupHandle, typename KvHandle, bool IsUserOut>
	class Flusher
	{
	public:
		using encoding					  = Encoding;
		using group_handle_type			  = GroupHandle;
		using kv_handle_type			  = KvHandle;

		constexpr static bool is_user_out = IsUserOut;

		using char_type					  = typename Encoding::char_type;
		using string_view_type			  = std::basic_string_view<char_type>;

	private:
		template<typename GH, bool>
		struct identifier
		{
		};

		template<typename GH>
		struct identifier<GH, false>
		{
			using type = decltype(std::declval<GH>().flush(std::declval<ini::ostream_type<char_type>&>(), std::declval<string_view_type>()).name);
		};

		template<typename GH>
		struct identifier<GH, true>
		{
			using type = decltype(std::declval<GH>().flush(std::declval<string_view_type>()).name);
		};

	public:
		using identifier_type	  = typename identifier<group_handle_type, is_user_out>::type;

		using comment_view_type	  = ini::comment_view_type<string_view_type>;

		using error_reporter_type = ErrorReporter<std::remove_cvref_t<Flusher>>;

		using buffer_type		  = typename error_reporter_type::buffer_type;
		using buffer_anchor_type  = typename error_reporter_type::buffer_anchor_type;
		using reader_type		  = typename error_reporter_type::reader_type;
		using position_type		  = typename error_reporter_type::position_type;
		using lexeme_type		  = typename error_reporter_type::lexeme_type;

		[[no_unique_address]] FlushFile<is_user_out, char_type> file_;

		group_handle_type										group_handle_;
		kv_handle_type											kv_handle_;

		comment_view_type										last_comment_;

#if defined(GAL_INI_DEBUG_GROUP)
		string_view_type current_group_;
#endif

		auto clear_last_comment() -> void
		{
			last_comment_ = {};
		}

		auto flush_last_comment() -> void
		{
			if (!last_comment_.empty())
			{
				// 'indication' 'space' comment '\n'
				if constexpr (is_user_out)
				{
					group_handle_.user()
							<< ini::make_comment_indication<string_view_type>(last_comment_.indication)
							<< ini::blank_separator<string_view_type> << last_comment_.comment
							<< ini::line_separator<string_view_type>;
				}
				else
				{
					file_
							<< ini::make_comment_indication<string_view_type>(last_comment_.indication)
							<< ini::blank_separator<string_view_type> << last_comment_.comment
							<< ini::line_separator<string_view_type>;
				}

				clear_last_comment();
			}
		}

		// [group_name] ; inline_comment
		auto flush_group_head(const string_view_type name, const comment_view_type inline_comment = {}) -> void
		{
			// ; last_comment <-- flush this
			// [group_name] ; inline_comment
			flush_last_comment();

			// [group_name] <-- flush this
			if constexpr (is_user_out)
			{
				kv_handle_ = group_handle_.flush(name);
			}
			else
			{
				kv_handle_ = group_handle_.flush(file_.out(), name);
			}

			// [group_name] ; inline_comment <-- flush this
			if (inline_comment.indication != ini::CommentIndication::INVALID)
			{
				// 'space' inline_comment_indication 'space' inline_comment
				if constexpr (is_user_out)
				{
					group_handle_.user()
							<< ini::blank_separator<string_view_type> << make_comment_indication<string_view_type>(inline_comment.indication)
							<< ini::blank_separator<string_view_type> << inline_comment.comment;
				}
				else
				{
					file_
							<< ini::blank_separator<string_view_type> << make_comment_indication<string_view_type>(inline_comment.indication)
							<< ini::blank_separator<string_view_type> << inline_comment.comment;
				}
			}

			// '\n'
			blank_line();
		}

		auto flush_kvs_remaining() -> void
		{
			if constexpr (is_user_out)
			{
				kv_handle_.flush_remaining();
			}
			else
			{
				kv_handle_.flush_remaining(file_.out());
			}
		}

		auto flush_group_remaining() -> void
		{
			if constexpr (is_user_out)
			{
				group_handle_.flush_remaining();
			}
			else
			{
				group_handle_.flush_remaining(file_.out());
			}
		}

	public:
		Flusher(
				const std::string_view file_path,
				group_handle_type	   group_handle) : file_{file_path},
												  group_handle_{group_handle},
												  kv_handle_{},
												  last_comment_{} {}

		Flusher(const Flusher&)					   = delete;
		Flusher(Flusher&&)						   = delete;
		auto operator=(const Flusher&) -> Flusher& = delete;
		auto operator=(Flusher&&) -> Flusher&	   = delete;

		~Flusher() noexcept
		{
			flush_kvs_remaining();
			flush_group_remaining();
		}

		// The parser ensures that if Flusher::comment is called, the indication must be valid.
		auto comment(
				const char_type	  indication,
				const lexeme_type context) -> void
		{
			// todo: test?
			const auto indication_type = ini::make_comment_indication<string_view_type>(indication);
			assert((indication_type != ini::CommentIndication::INVALID) && "INVALID_COMMENT");

			const string_view_type user_comment_context{context.data(), context.size()};
			last_comment_ = ini::make_comment_view<string_view_type>(indication_type, user_comment_context);
		}

		// The parser will pass an empty inline_comment if it does not resolve it (unlike Flusher::comment).
		auto group(
				[[maybe_unused]] const position_type	position,
				const lexeme_type						group_name,
				const std::pair<char_type, lexeme_type> inline_comment) -> void
		{
			flush_kvs_remaining();

			if (const string_view_type user_group_name{group_name.data(), group_name.size()};
				group_handle_.contains(user_group_name))
			{
#if defined(GAL_INI_DEBUG_GROUP)
				current_group_ = user_group_name;
#endif

				const auto			   indication_type = ini::make_comment_indication<string_view_type>(inline_comment.first);
				const string_view_type user_comment_context{inline_comment.second.data(), inline_comment.second.size()};

				flush_group_head(
						user_group_name,
						ini::make_comment_view<string_view_type>(
								indication_type,
								user_comment_context));
			}
		}

		auto value(
				[[maybe_unused]] const position_type	position,
				const lexeme_type						variable_key,
				[[maybe_unused]] const lexeme_type		variable_value,
				const std::pair<char_type, lexeme_type> inline_comment) -> void
		{
			if (const string_view_type user_key{variable_key.data(), variable_key.size()};
				kv_handle_.contains(user_key))
			{
				flush_last_comment();

				if constexpr (is_user_out)
				{
					kv_handle_.flush(user_key);
				}
				else
				{
					kv_handle_.flush(file_.out(), user_key);
				}

				// key = value ; inline_comment <-- flush this
				if (const auto indication = ini::make_comment_indication<string_view_type>(inline_comment.first);
					indication != ini::CommentIndication::INVALID)
				{
					const string_view_type user_comment_context{inline_comment.second.data(), inline_comment.second.size()};

					// 'space' inline_comment_indication 'space' inline_comment
					if constexpr (is_user_out)
					{
						group_handle_.user()
								<< ini::blank_separator<string_view_type> << inline_comment.first
								<< ini::blank_separator<string_view_type> << user_comment_context;
					}
					else
					{
						file_
								<< ini::blank_separator<string_view_type> << inline_comment.first
								<< ini::blank_separator<string_view_type> << user_comment_context;
					}
				}

				// '\n'
				blank_line();
			}
		}

		auto blank_line() -> void
		{
			if constexpr (is_user_out)
			{
				group_handle_.user() << ini::line_separator<string_view_type>;
			}
			else
			{
				file_ << ini::line_separator<string_view_type>;
			}
		}
	};
}// namespace

namespace gal::ini
{
	namespace extractor_detail
	{
		namespace
		{
			template<typename State>
			[[nodiscard]] auto do_extract_from_file(
					std::string_view							 file_path,
					group_append_type<typename State::char_type> group_appender) -> ExtractResult
			{
				if (!std::filesystem::exists(file_path))
				{
					return ExtractResult::FILE_NOT_FOUND;
				}

				if (auto file = lexy::read_file<typename State::encoding>(file_path.data());
					!file)
				{
					switch (file.error())
					{
						case lexy::file_error::file_not_found:
						{
							return ExtractResult::FILE_NOT_FOUND;
						}
						case lexy::file_error::permission_denied:
						{
							return ExtractResult::PERMISSION_DENIED;
						}
						case lexy::file_error::os_error:
						{
							return ExtractResult::INTERNAL_ERROR;
						}
						case lexy::file_error::_success:
						default:
						{
							GAL_INI_UNREACHABLE();
						}
					}
				}
				else
				{
					State state{{file.buffer().data(), file.buffer().size()}, file_path, group_appender};

					parse(state, typename State::buffer_type{file.buffer().data(), file.buffer().size()}, file_path);

					return ExtractResult::SUCCESS;
				}
			}

			template<typename State>
			[[nodiscard]] auto do_extract_from_buffer(
					std::basic_string_view<typename State::char_type> buffer,
					group_append_type<typename State::char_type>	  group_appender) -> ExtractResult
			{
				State state{{buffer.data(), buffer.size()}, State::error_reporter_type::buffer_file_path, group_appender};

				parse(state, typename State::buffer_type{buffer.data(), buffer.size()}, State::error_reporter_type::buffer_file_path);

				return ExtractResult::SUCCESS;
			}
		}// namespace

		// char
		[[nodiscard]] auto extract_from_file(
				const std::string_view		  file_path,
				const group_append_type<char> group_appender) -> ExtractResult
		{
			using char_type = char;
			// todo: encoding?
			using encoding	= lexy::utf8_char_encoding;

			return do_extract_from_file<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					file_path,
					group_appender);
		}

		// wchar_t
		//	 [[nodiscard]] auto extract_from_file(
		//	 		const std::string_view		   file_path,
		//	 		const group_append_type<wchar_t> group_appender) -> ExtractResult
		//	 {
		//	 	using char_type = wchar_t;
		//	 	// todo: encoding?
		//	 	using encoding	= lexy::utf32_encoding;
		//
		//	 	return do_extract_from_file<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
		//	 			file_path,
		//	 			group_appender);
		//	 }

		// char8_t
		[[nodiscard]] auto extract_from_file(
				const std::string_view			 file_path,
				const group_append_type<char8_t> group_appender) -> ExtractResult
		{
			using char_type = char8_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_file<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					file_path,
					group_appender);
		}

		// char16_t
		[[nodiscard]] auto extract_from_file(
				const std::string_view			  file_path,
				const group_append_type<char16_t> group_appender) -> ExtractResult
		{
			using char_type = char16_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_file<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					file_path,
					group_appender);
		}

		// char32_t
		[[nodiscard]] auto extract_from_file(
				const std::string_view			  file_path,
				const group_append_type<char32_t> group_appender) -> ExtractResult
		{
			using char_type = char32_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_file<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					file_path,
					group_appender);
		}

		// char
		[[nodiscard]] auto extract_from_buffer(
				const std::basic_string_view<char> buffer,
				const group_append_type<char>	   group_appender) -> ExtractResult
		{
			using char_type = char;
			// todo: encoding?
			using encoding	= lexy::utf8_char_encoding;

			return do_extract_from_buffer<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					buffer,
					group_appender);
		}

		// wchar_t
		//	[[nodiscard]] auto extract_from_buffer(
		//			const std::basic_string_view<wchar_t> buffer,
		//			const group_append_type<wchar_t>		group_appender) -> ExtractResult
		//	{
		//		using char_type = wchar_t;
		//		// todo: encoding?
		//		using encoding	= lexy::utf32_encoding;
		//
		//		return do_extract_from_buffer<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
		//				buffer,
		//				group_appender);
		//	}

		// char8_t
		[[nodiscard]] auto extract_from_buffer(
				const std::basic_string_view<char8_t> buffer,
				const group_append_type<char8_t>	  group_appender) -> ExtractResult
		{
			using char_type = char8_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_buffer<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					buffer,
					group_appender);
		}

		// char16_t
		[[nodiscard]] auto extract_from_buffer(
				const std::basic_string_view<char16_t> buffer,
				const group_append_type<char16_t>	   group_appender) -> ExtractResult
		{
			using char_type = char16_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_buffer<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					buffer,
					group_appender);
		}

		// char32_t
		[[nodiscard]] auto extract_from_buffer(
				const std::basic_string_view<char32_t> buffer,
				const group_append_type<char32_t>	   group_appender) -> ExtractResult
		{
			using char_type = char32_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_extract_from_buffer<Extractor<encoding, group_append_type<char_type>, kv_append_type<char_type>>>(
					buffer,
					group_appender);
		}
	}// namespace extractor_detail

	namespace flusher_detail
	{
		namespace
		{
			template<typename State>
			[[nodiscard]] auto do_flush(
					std::string_view				  file_path,
					typename State::group_handle_type group_handler) -> FlushResult
			{
				if (!std::filesystem::exists(file_path))
				{
					// The file doesn't exist, it doesn't matter, just write the file.
					// return FlushResult::FILE_NOT_FOUND;
					State state{file_path, group_handler};
					return FlushResult::SUCCESS;
				}

				if (auto file = lexy::read_file<typename State::encoding>(file_path.data());
					!file)
				{
					switch (file.error())
					{
						case lexy::file_error::file_not_found:
						{
							// return FlushResult::FILE_NOT_FOUND;
							[[fallthrough]];
						}
						case lexy::file_error::permission_denied:
						{
							return FlushResult::PERMISSION_DENIED;
						}
						case lexy::file_error::os_error:
						{
							return FlushResult::INTERNAL_ERROR;
						}
						case lexy::file_error::_success:
						default:
						{
							GAL_INI_UNREACHABLE();
						}
					}
				}
				else
				{
					State state{file_path, group_handler};

					parse(state, {file.buffer().data(), file.buffer().size()}, file_path);

					return FlushResult::SUCCESS;
				}
			}
		}// namespace

		// char
		[[nodiscard]] auto flush_to_file(
				const std::string_view			 file_path,
				const group_ostream_handle<char> group_handler) -> FlushResult
		{
			using char_type = char;
			// todo: encoding?
			using encoding	= lexy::utf8_char_encoding;

			return do_flush<Flusher<encoding, group_ostream_handle<char_type>, kv_ostream_handle<char_type>, false>>(
					file_path,
					group_handler);
		}

		// wchar_t
		//[[nodiscard]] auto flush_to_file(
		//		const std::string_view	  file_path,
		//		const group_ostream_handle<wchar_t> group_handler) -> FlushResult
		//{
		//	using char_type = wchar_t;
		//	// todo: encoding?
		//	using encoding	= lexy::utf32_encoding;

		//	return do_flush<Flusher<encoding, group_ostream_handle<char_type>, kv_ostream_handle<char_type>, false>>(
		//			file_path,
		//			group_handler);
		//}

		// char8_t
		[[nodiscard]] auto flush_to_file(
				const std::string_view				file_path,
				const group_ostream_handle<char8_t> group_handler) -> FlushResult
		{
			using char_type = char8_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_ostream_handle<char_type>, kv_ostream_handle<char_type>, false>>(
					file_path,
					group_handler);
		}

		// char16_t
		[[nodiscard]] auto flush_to_file(
				const std::string_view				 file_path,
				const group_ostream_handle<char16_t> group_handler) -> FlushResult
		{
			using char_type = char16_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_ostream_handle<char_type>, kv_ostream_handle<char_type>, false>>(
					file_path,
					group_handler);
		}

		// char32_t
		[[nodiscard]] auto flush_to_file(
				const std::string_view				 file_path,
				const group_ostream_handle<char32_t> group_handler) -> FlushResult
		{
			using char_type = char32_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_ostream_handle<char_type>, kv_ostream_handle<char_type>, false>>(
					file_path,
					group_handler);
		}

		// char
		[[nodiscard]] auto flush_to_user(
				const std::string_view		  file_path,
				const group_user_handle<char> group_handler) -> FlushResult
		{
			using char_type = char;
			// todo: encoding?
			using encoding	= lexy::utf8_char_encoding;

			return do_flush<Flusher<encoding, group_user_handle<char_type>, kv_user_handle<char_type>, true>>(
					file_path,
					group_handler);
		}

		// wchar_t
		//[[nodiscard]] auto flush_to_user(
		//		const std::string_view	  file_path,
		//		const group_user_handle<wchar_t> group_handler) -> FlushResult
		//{
		//	using char_type = wchar_t;
		//	// todo: encoding?
		//	using encoding	= lexy::utf32_encoding;

		//	return do_flush<Flusher<encoding, group_user_handle<char_type>, kv_user_handle<char_type>, true>>(
		//			file_path,
		//			group_handler);
		//}

		// char8_t
		[[nodiscard]] auto flush_to_user(
				const std::string_view			 file_path,
				const group_user_handle<char8_t> group_handler) -> FlushResult
		{
			using char_type = char8_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_user_handle<char_type>, kv_user_handle<char_type>, true>>(
					file_path,
					group_handler);
		}

		// char16_t
		[[nodiscard]] auto flush_to_user(
				const std::string_view			  file_path,
				const group_user_handle<char16_t> group_handler) -> FlushResult
		{
			using char_type = char16_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_user_handle<char_type>, kv_user_handle<char_type>, true>>(
					file_path,
					group_handler);
		}

		// char32_t
		[[nodiscard]] auto flush_to_user(
				const std::string_view			  file_path,
				const group_user_handle<char32_t> group_handler) -> FlushResult
		{
			using char_type = char32_t;
			using encoding	= lexy::deduce_encoding<char_type>;

			return do_flush<Flusher<encoding, group_user_handle<char_type>, kv_user_handle<char_type>, true>>(
					file_path,
					group_handler);
		}
	}// namespace flusher_detail
}// namespace gal::ini