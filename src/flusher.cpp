#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ini/extractor.hpp>
#include <ini/flusher.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>
#include <unordered_set>
#include <utility>

//#define GAL_INI_TRACE_FLUSH

#ifdef GAL_INI_TRACE_FLUSH
	#include <lexy/visualize.hpp>
#endif

namespace
{
	namespace ini = gal::ini;

	template<typename StringType>
	[[nodiscard]] auto to_char_string(const StringType& string) -> decltype(auto)
	{
		// todo: char8_t/char16_t/char32_t
		if constexpr (std::is_same_v<typename StringType::value_type, char>) { return std::string_view{string.data(), string.size()}; }
		else { return std::filesystem::path{string.data(), string.data() + string.size()}.string(); }
	}

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

	using file_path_type = std::string_view;

	template<typename Char>
	class FlushFile
	{
	public:
		using path_type = std::filesystem::path;
		using out_type	= std::basic_ofstream<Char>;

	private:
		path_type source_path_;
		path_type temp_path_;
		out_type  out_;

	public:
		explicit FlushFile(const file_path_type file_path)
			: source_path_{file_path},
			  temp_path_{
					  std::filesystem::temp_directory_path() /
					  // todo: Do we need a unique file name? Because if we open the file in std::ios::trunc mode and write less data than before, some unwanted garbage data will be left in the file.
					  source_path_.stem().string().append(std::to_string(reinterpret_cast<std::uintptr_t>(&source_path_))).append(source_path_.extension().string())
					  // source_path_.filename()
			  },
			  out_{temp_path_, std::ios::out | std::ios::trunc}
		{
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

		[[nodiscard]] auto ready() const noexcept -> bool { return exists(source_path_) && out_.is_open() && out_.good(); }

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

	template<typename Char, typename GroupHandle, typename KvHandle, typename Encoding>
	class FlusherState
	{
	public:
		using group_handle_type = GroupHandle;
		using kv_handle_type	= KvHandle;
		using encoding			= Encoding;

		using reader_type		= decltype(std::declval<const lexy::buffer<encoding>&>().reader());

		using string_view_type	= std::basic_string_view<Char>;

		using char_type			= Char;
		using position_type		= const char_type*;

		using comment_view_type = ini::comment_view_type<string_view_type>;

	private:
		FlushFile<char_type> file_;

		group_handle_type	 group_handle_;
		kv_handle_type		 kv_handle_;

		comment_view_type	 last_comment_;

		auto				 clear_last_comment() -> void { last_comment_ = {}; }

		auto				 flush_last_comment() -> void
		{
			if (!last_comment_.empty())
			{
				// 'indication' 'space' comment '\n'
				file_
						<< ini::make_comment_indication<string_view_type>(last_comment_.indication)
						<< ini::blank_separator<string_view_type> << last_comment_.comment
						<< ini::line_separator<string_view_type>;
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
			kv_handle_ = group_handle_.flush(file_.out(), name);

			// [group_name] ; inline_comment <-- flush this
			if (!inline_comment.empty())
			{
				// 'space' inline_comment_indication 'space' inline_comment
				file_
						<< ini::blank_separator<string_view_type> << make_comment_indication<string_view_type>(inline_comment.indication)
						<< ini::blank_separator<string_view_type> << inline_comment.comment;
			}

			// '\n'
			file_ << ini::line_separator<string_view_type>;
		}

		auto flush_kvs_remaining() -> void
		{
			kv_handle_.flush_remaining(file_.out());
		}

		auto flush_group_remaining() -> void
		{
			group_handle_.flush_remaining(file_.out());
		}

	public:
		FlusherState(
				const file_path_type file_path,
				group_handle_type	 group_handle)
			: file_{file_path},
			  group_handle_{group_handle},
			  kv_handle_{} {}

		FlusherState(const FlusherState&)					 = delete;
		FlusherState(FlusherState&&)						 = delete;
		auto operator=(const FlusherState&) -> FlusherState& = delete;
		auto operator=(FlusherState&&) -> FlusherState&		 = delete;

		~FlusherState() noexcept
		{
			flush_kvs_remaining();
			flush_group_remaining();
		}

		[[nodiscard]] auto ready() const -> bool { return file_.ready(); }

		auto			   comment(const comment_view_type comment) -> void
		{
			last_comment_ = comment;
		}

		auto group(const position_type position, const string_view_type group_name, const comment_view_type inline_comment = {}) -> void
		{
			(void)position;

			flush_kvs_remaining();

			if (group_handle_.contains(group_name))
			{
				flush_group_head(group_name, inline_comment);
			}
		}

		auto value(const position_type position, const string_view_type key, const string_view_type value, const comment_view_type inline_comment = {}) -> void
		{
			(void)position;
			(void)value;

			if (kv_handle_.contains(key))
			{
				flush_last_comment();

				kv_handle_.flush(file_.out(), key);

				// key = value ; inline_comment <-- flush this
				if (!inline_comment.empty())
				{
					// 'space' inline_comment_indication 'space' inline_comment
					file_
							<< ini::blank_separator<string_view_type> << make_comment_indication<string_view_type>(inline_comment.indication)
							<< ini::blank_separator<string_view_type> << inline_comment.comment;
				}

				// '\n'
				file_ << ini::line_separator<string_view_type>;
			}
		}

		auto blank_line() -> void { file_ << ini::line_separator<string_view_type>; }
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		template<typename State>
		struct comment_context
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[comment context]"; }

			constexpr static auto				rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n' and '\r\n'
							// todo: multi-line comment
							dsl::unicode::print - dsl::unicode::newline);

			constexpr static auto value = lexy::as_string<typename State::string_view_type, typename State::encoding>;
			// lexy::callback<typename State::string_view_type>(
			// 		[](const lexy::lexeme<typename State::reader_type> lexeme) -> typename State::string_view_type
			// 		{ return {reinterpret_cast<const typename State::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		template<typename State, typename State::char_type Indication>
		struct comment
		{
			[[nodiscard]] consteval static auto		   name() noexcept -> const char* { return "[comment]"; }

			constexpr static typename State::char_type indication = Indication;

			constexpr static auto					   rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse comment begin") +
					 dsl::p<comment_context<State>> +
					 LEXY_DEBUG("parse comment end") +
					 dsl::newline);

			constexpr static auto value = callback<void>(
					[](State& state, const typename State::string_view_type context) -> void
					{ state.comment({.indication = ini::make_comment_indication<typename State::string_view_type>(indication), .comment = context}); });
		};

		template<typename State, typename State::char_type Indication>
		struct comment_inline
		{
			[[nodiscard]] consteval static auto		   name() noexcept -> const char* { return "[inline comment]"; }

			// constexpr static char indication = comment<ParseState, Indication>::indication;
			constexpr static typename State::char_type indication = Indication;

			// constexpr static auto rule		 = comment<ParseState, Indication>::rule;
			constexpr static auto					   rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse inline_comment begin") +
					 dsl::p<comment_context<State>> +
					 LEXY_DEBUG("parse inline_comment end"));

			constexpr static auto value = callback<typename State::comment_view_type>(
					[]([[maybe_unused]] State& state, const typename State::string_view_type context) -> typename State::comment_view_type
					{ return {.indication = ini::make_comment_indication<typename State::string_view_type>(indication), .comment = context}; });
		};

		template<typename State>
		using comment_hash_sign = comment<State, ini::comment_indication_hash_sign<typename State::string_view_type>>;
		template<typename State>
		using comment_semicolon = comment<State, ini::comment_indication_semicolon<typename State::string_view_type>>;

		template<typename State>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<State>> |
				dsl::p<comment_semicolon<State>>;

		template<typename State>
		using comment_inline_hash_sign = comment_inline<State, ini::comment_indication_hash_sign<typename State::string_view_type>>;
		template<typename State>
		using comment_inline_semicolon = comment_inline<State, ini::comment_indication_semicolon<typename State::string_view_type>>;

		template<typename State>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<State>> |
				dsl::p<comment_inline_semicolon<State>>;

		template<typename State>
		struct group_name
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group name]"; }

			constexpr static auto				rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
							dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

			constexpr static auto value = lexy::as_string<typename State::string_view_type, typename State::encoding>;
		};

		template<typename State>
		struct variable_key
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[key]"; }

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

			constexpr static auto value = lexy::as_string<typename State::string_view_type, typename State::encoding>;
		};

		template<typename State>
		struct variable_value
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[value]"; }

			constexpr static auto				rule = []
			{
				// begin with not '\r', '\n', '\r\n', whitespace or '='
				constexpr auto begin_with_not_blank	   = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
				// continue with printable, but excluding '\r', '\n', '\r\n', whitespace and '='
				constexpr auto continue_with_printable = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

				return dsl::peek(begin_with_not_blank) >> dsl::identifier(begin_with_not_blank, continue_with_printable);
			}();

			constexpr static auto value = lexy::as_string<typename State::string_view_type, typename State::encoding>;
		};

		// identifier = [variable] ; inline comment
		template<typename State>
		struct variable_declaration
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

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
					[]([[maybe_unused]] State& state) {},
					// [identifier] = [variable] [comment]
					[](State& state, const typename State::position_type position, const typename State::string_view_type key, const typename State::string_view_type value, typename State::comment_view_type comment) -> void
					{ state.value(position, key, value, comment); },
					// [identifier] = [] [variable]
					[](State& state, const typename State::position_type position, const typename State::string_view_type key, const typename State::string_view_type value, lexy::nullopt) -> void
					{ state.value(position, key, value); },
					// [identifier] = [] [comment]
					[](State& state, const typename State::position_type position, const typename State::string_view_type key, lexy::nullopt, typename State::comment_view_type comment) -> void
					{ state.value(position, key, typename State::string_view_type{}, comment); },
					// [identifier] = [] []
					[](State& state, const typename State::position_type position, const typename State::string_view_type key, lexy::nullopt, lexy::nullopt) -> void
					{ state.value(position, key, typename State::string_view_type{}); });
		};

		template<typename State>
		struct blank_line
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[empty line]"; }

			constexpr static auto				rule =
					dsl::peek(dsl::newline | dsl::unicode::blank) >>
					(LEXY_DEBUG("empty line") +
					 dsl::until(dsl::newline).or_eof());

			constexpr static auto value = callback<void>(
					[](State& state) -> void
					{ state.blank_line(); });
		};

		template<typename State>
		struct variable_or_comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[variable or comment]"; }

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
		struct group_declaration
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group declaration]"; }

			struct header : lexy::transparent_production
			{
				[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group head]"; }

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
						// [group_name] [comment]
						[](State& state, const typename State::position_type position, const typename State::string_view_type group_name, typename State::comment_view_type comment) -> void
						{ state.group(position, group_name, comment); },
						// [group_name] []
						[](State& state, const typename State::position_type position, const typename State::string_view_type group_name, lexy::nullopt) -> void
						{ state.group(position, group_name); });
			};

			// end with 'eof' or next '[' (group begin)
			constexpr static auto rule =
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
		struct file
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[file context]"; }

			constexpr static auto				whitespace = dsl::ascii::blank;

			constexpr static auto				rule	   = dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<State>>);

			constexpr static auto				value	   = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename State, typename Buffer>
	auto flush(State& state, const Buffer& buffer, std::string_view file_path) -> void
	{
#ifdef GAL_INI_TRACE_FLUSH
		// This must come before the parse, because our handler is "non-reentrant".
		lexy::trace<grammar::file<State>>(
				stderr,
				buffer,
				{.flags = lexy::visualize_fancy});
#endif

		if (const auto result =
					lexy::parse<grammar::file<State>>(
							buffer,
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_path.data()));
			!result.has_value())
		{
			// todo: error ?
		}
	}
}// namespace

namespace gal::ini::flusher_detail
{
	namespace
	{
		template<typename State>
		[[nodiscard]] auto flush_to_file(
				std::string_view						file_path,
				group_handle<typename State::char_type> group_handler) -> FlushResult
		{
			std::filesystem::path path{file_path};
			if (!exists(path))
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

				flush(state, file.buffer(), file_path);

				return FlushResult::SUCCESS;
			}
		}
	}// namespace

	// char
	[[nodiscard]] auto flush_to_file(
			std::string_view   file_path,
			group_handle<char> group_handler) -> FlushResult
	{
		using char_type = char;
		// todo: encoding?
		using encoding	= lexy::utf8_char_encoding;

		return flush_to_file<FlusherState<char_type, group_handle<char_type>, kv_handle<char_type>, encoding>>(
				file_path,
				group_handler);
	}

	// wchar_t
	//[[nodiscard]] auto flush_to_file(
	//		std::string_view	  file_path,
	//		group_handle<wchar_t> group_handler) -> FlushResult
	//{
	//	using char_type = wchar_t;
	//	// todo: encoding?
	//	using encoding	= lexy::utf32_encoding;

	//	return flush_to_file<FlusherState<char_type, group_handle<char_type>, kv_handle<char_type>, encoding>>(
	//			file_path,
	//			group_handler);
	//}

	// char8_t
	[[nodiscard]] auto flush_to_file(
			std::string_view	  file_path,
			group_handle<char8_t> group_handler) -> FlushResult
	{
		using char_type = char8_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return flush_to_file<FlusherState<char_type, group_handle<char_type>, kv_handle<char_type>, encoding>>(
				file_path,
				group_handler);
	}

	// char16_t
	[[nodiscard]] auto flush_to_file(
			std::string_view	   file_path,
			group_handle<char16_t> group_handler) -> FlushResult
	{
		using char_type = char16_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return flush_to_file<FlusherState<char_type, group_handle<char_type>, kv_handle<char_type>, encoding>>(
				file_path,
				group_handler);
	}

	// char32_t
	[[nodiscard]] auto flush_to_file(
			std::string_view	   file_path,
			group_handle<char32_t> group_handler) -> FlushResult
	{
		using char_type = char32_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return flush_to_file<FlusherState<char_type, group_handle<char_type>, kv_handle<char_type>, encoding>>(
				file_path,
				group_handler);
	}
}// namespace gal::ini::flusher_detail
