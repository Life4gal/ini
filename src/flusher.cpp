#include <filesystem>
#include <fstream>
#include <functional>
#include <ini/ini.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>
#include <ranges>
#include <unordered_set>

//#define GAL_INI_TRACE_PARSE

#ifdef GAL_INI_TRACE_PARSE
	#include <lexy/visualize.hpp>
#endif

namespace
{
	namespace ini = gal::ini;

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

	using default_encoding = lexy::utf8_encoding;
	using reader_type	   = decltype(std::declval<const lexy::buffer<default_encoding>&>().reader());

	class AutoFile
	{
	public:
		using path_type = std::filesystem::path;
		using out_type	= std::ofstream;

	private:
		path_type source_path_;
		path_type temp_path_;
		out_type  out_;

	public:
		explicit AutoFile(const ini::IniExtractor::file_path_type file_path)
			: source_path_{file_path},
			  temp_path_{std::filesystem::temp_directory_path() / source_path_.filename()},
			  out_{temp_path_, std::ios::out | std::ios::trunc} {}

		AutoFile(const AutoFile&)					 = delete;
		AutoFile(AutoFile&&)						 = delete;
		auto operator=(const AutoFile&) -> AutoFile& = delete;
		auto operator=(AutoFile&&) -> AutoFile&		 = delete;

		~AutoFile() noexcept
		{
			out_.close();

			std::filesystem::copy_file(
					temp_path_,
					source_path_,
					std::filesystem::copy_options::overwrite_existing);
		}

		[[nodiscard]] auto ready() const -> bool { return exists(source_path_) && out_.is_open() && out_.good(); }

		template<typename Data>
			requires requires {
						 out_ << std::declval<Data>();
					 }
		auto operator<<(const Data& data) -> AutoFile&
		{
			out_ << data;
			return *this;
		}
	};

	template<bool KeepComments, bool KeepEmptyGroup>
	class FlusherState
	{
	public:
		using flusher_type					   = ini::impl::IniFlusher;
		using write_type					   = flusher_type::write_type;
		using group_type					   = flusher_type::group_type;
		using context_type					   = flusher_type::context_type;

		// using pending_flush_group_type		   = decltype(std::declval<const context_type&>() | std::views::keys | std::views::transform([](const auto& string) -> ini::string_view_type { return string; }));
		using pending_flush_group_type		   = std::unordered_set<ini::string_view_type, ini::string_hash_type, std::equal_to<>>;

		constexpr static bool keep_comments	   = KeepComments;
		constexpr static bool keep_empty_group = KeepEmptyGroup;

	private:
		AutoFile					file_;

		const context_type&			context_;
		std::unique_ptr<write_type> writer_;
		pending_flush_group_type	pending_flush_group_;

		ini::comment_view_type		last_comment_;

		auto						clear_last_comment() -> void { last_comment_ = {}; }

		auto						flush_last_comment() -> void
		{
			if constexpr (keep_comments)
			{
				if (!last_comment_.empty())
				{
					file_ << make_comment_indication(last_comment_.indication) << ini::blank_separator << last_comment_.comment << ini::line_separator;
					clear_last_comment();
				}
			}
		}

		// [group_name] ; inline_comment
		auto flush_group_head(const ini::string_view_type name, const ini::comment_view_type inline_comment = {}) -> void
		{
			// ; last_comment <-- flush this
			// [group_name] ; inline_comment
			flush_last_comment();

			file_ << ini::square_bracket.first << name << ini::square_bracket.second;
			if constexpr (keep_comments)
			{
				if (!inline_comment.empty())
				{
					file_ << ini::blank_separator << make_comment_indication(inline_comment.indication) << ini::blank_separator << inline_comment.comment;
				}
			}
			file_ << ini::line_separator;
		}

		auto flush_group_remainder() -> void
		{
			if (writer_) { writer_->flush_remainder(file_); }
		}

		auto flush_context_remainder() -> void
		{
			// First check that write is not pointing to legal memory,
			// if it is, this means that we have successfully read valid data from the file before,
			// which also means that writing an extra line_separator will not cause problems in the next read.
			if (writer_)
			{
				file_ << ini::line_separator;
			}

			for (const auto& name: pending_flush_group_)
			{
				const auto it = ini::impl::table_finder{}(context_, name);
				if (it == context_.end()) [[unlikely]]
				{
					// todo: impossible!
					continue;
				}

				write_type flusher{it->second};

				flush_group_head(name);
				flusher.flush_remainder(file_)
						// This extra line_separator is to make the data 'look' less crowded.
						<< ini::line_separator;
			}
		}

	public:
		explicit FlusherState(const ini::IniExtractor::file_path_type file_path, const context_type& context)
			: file_{file_path},
			  context_{context},
			  writer_{nullptr},
			  last_comment_{ini::make_comment_view(ini::CommentIndication::INVALID, {})}
		{
#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG)
			for (const auto& [group_name, _]: context_)
			{
				pending_flush_group_.insert(group_name);
			}
#else
			for (const auto& group_name: context_ | std::views::keys)
			{
				pending_flush_group_.insert(group_name);
			}
#endif
		}

		FlusherState(const FlusherState&)					 = delete;
		FlusherState(FlusherState&&)						 = delete;
		auto operator=(const FlusherState&) -> FlusherState& = delete;
		auto operator=(FlusherState&&) -> FlusherState&		 = delete;

		~FlusherState() noexcept
		{
			flush_group_remainder();
			flush_context_remainder();
		}

		[[nodiscard]] auto ready() const -> bool { return file_.ready(); }

		auto			   comment(const ini::comment_view_type comment) -> void
		{
			if constexpr (keep_comments) { last_comment_ = comment; }
		}

		auto begin_group(const ini::string_view_type group_name, const ini::comment_view_type inline_comment = {}) -> void
		{
			flush_group_remainder();

			writer_ = std::make_unique<write_type>(flusher_type{context_}.flush(group_name));
			pending_flush_group_.erase(group_name);

			if (writer_->empty())
			{
				writer_.reset();

				if constexpr (keep_empty_group) { flush_group_head(group_name, inline_comment); }
				else { clear_last_comment(); }
			}
			else { flush_group_head(group_name, inline_comment); }
		}

		auto value(const ini::string_view_type key, const ini::comment_view_type inline_comment = {}) -> void
		{
			if (writer_ && writer_->contains(key))
			{
				flush_last_comment();
				writer_->flush(key, file_);

				if constexpr (keep_comments)
				{
					if (!inline_comment.empty()) { file_ << ini::blank_separator << make_comment_indication(inline_comment.indication) << ini::blank_separator << inline_comment.comment; }
				}
				blank_line();
			}
			else { clear_last_comment(); }
		}

		auto blank_line() -> void { file_ << ini::line_separator; }
	};

	template<bool KeepEmptyGroup>
	class FlusherStateWithComment
	{
	public:
		using flusher_type					   = ini::impl::IniFlusherWithComment;
		using write_type					   = flusher_type::write_type;
		using group_type					   = flusher_type::group_type;
		using context_type					   = flusher_type::context_type;

		// using pending_flush_group_type		   = decltype(std::declval<const context_type&>() | std::views::keys | std::views::transform([](const auto& string) -> ini::string_view_type { return string; }));
		using pending_flush_group_type		   = std::unordered_set<ini::string_view_type, ini::string_hash_type, std::equal_to<>>;

		constexpr static bool keep_empty_group = KeepEmptyGroup;

	private:
		AutoFile					file_;

		const context_type&			context_;
		std::unique_ptr<write_type> writer_;
		pending_flush_group_type	pending_flush_group_;

		// ; comment
		// [group_name] ; inline_comment
		auto						flush_group_head(write_type& flusher, const ini::string_view_type name) -> void
		{
			// ; last_comment <-- flush this
			// [group_name] ; inline_comment
			if (flusher.has_comment())
			{
				const auto& comment = flusher.comment();
				file_ << ini::make_comment_indication(comment.indication) << ini::blank_separator << comment.comment << ini::line_separator;
			}

			file_ << ini::square_bracket.first << name << ini::square_bracket.second;

			// ; last_comment
			// [group_name] ; inline_comment <-- flush this
			if (flusher.has_inline_comment())
			{
				const auto& inline_comment = flusher.inline_comment();
				file_ << ini::blank_separator << ini::make_comment_indication(inline_comment.indication) << ini::blank_separator << inline_comment.comment;
			}

			file_ << ini::line_separator;
		}

		auto flush_group_remainder() -> void
		{
			if (writer_) { writer_->flush_remainder(file_); }
		}

		auto flush_context_remainder() -> void
		{
			// First check that write is not pointing to legal memory,
			// if it is, this means that we have successfully read valid data from the file before,
			// which also means that writing an extra line_separator will not cause problems in the next read.
			if (writer_)
			{
				file_ << ini::line_separator;
			}

			for (const auto& name: pending_flush_group_)
			{
				const auto it = ini::impl::table_finder{}(context_, name);
				if (it == context_.end()) [[unlikely]]
				{
					// todo: impossible!
					continue;
				}

				write_type flusher{it->second};

				flush_group_head(flusher, name);
				flusher.flush_remainder(file_)
						// This extra line_separator is to make the data 'look' less crowded.
						<< ini::line_separator;
			}
		}

	public:
		explicit FlusherStateWithComment(const ini::IniExtractorWithComment::file_path_type file_path, const context_type& context)
			: file_{file_path},
			  context_{context},
			  writer_{nullptr}
		{
#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG)
			for (const auto& [group_name, _]: context_)
			{
				pending_flush_group_.insert(group_name);
			}
#else
			for (const auto& group_name: context_ | std::views::keys)
			{
				pending_flush_group_.insert(group_name);
			}
#endif
		}

		FlusherStateWithComment(const FlusherStateWithComment&)					   = delete;
		FlusherStateWithComment(FlusherStateWithComment&&)						   = delete;
		auto operator=(const FlusherStateWithComment&) -> FlusherStateWithComment& = delete;
		auto operator=(FlusherStateWithComment&&) -> FlusherStateWithComment&	   = delete;

		~FlusherStateWithComment() noexcept
		{
			flush_group_remainder();
			flush_context_remainder();
		}

		[[nodiscard]] auto ready() const -> bool { return file_.ready(); }

		auto			   comment([[maybe_unused]] const ini::comment_view_type comment) const -> void { (void)this; }

		auto			   begin_group(const ini::string_view_type group_name, [[maybe_unused]] const ini::comment_view_type inline_comment = {}) -> void
		{
			flush_group_remainder();

			writer_ = std::make_unique<write_type>(flusher_type{context_}.flush(group_name));
			pending_flush_group_.erase(group_name);

			if (writer_->empty())
			{
				if constexpr (keep_empty_group) { flush_group_head(*writer_, group_name); }

				writer_.reset();
			}
			else { flush_group_head(*writer_, group_name); }
		}

		auto value(const ini::string_view_type key, [[maybe_unused]] const ini::comment_view_type inline_comment = {}) -> void
		{
			if (!writer_) {}
			else { writer_->flush(key, file_); }
		}

		auto blank_line() -> void { file_ << ini::line_separator; }
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

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

			constexpr static auto value =// lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type
							{ return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		template<typename ParseState, auto Indication>
		struct comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[comment]"; }

			constexpr static auto				indication = Indication;

			constexpr static auto				rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse comment begin") +
					 dsl::p<comment_context> +
					 LEXY_DEBUG("parse comment end") +
					 dsl::newline);

			constexpr static auto value = callback<void>(
					[](ParseState& state, const ini::string_view_type context) -> void
					{ state.comment({.indication = ini::make_comment_indication(indication), .comment = context}); });
		};

		template<typename ParseState, auto Indication>
		struct comment_inline
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[inline comment]"; }

			// constexpr static char indication = comment<ParseState, Indication>::indication;
			constexpr static auto				indication = Indication;

			// constexpr static auto rule		 = comment<ParseState, Indication>::rule;
			constexpr static auto				rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse inline_comment begin") +
					 dsl::p<comment_context> +
					 LEXY_DEBUG("parse inline_comment end"));

			constexpr static auto value = callback<ini::comment_view_type>(
					[]([[maybe_unused]] ParseState& state, const ini::string_view_type context) -> ini::comment_view_type
					{ return {.indication = ini::make_comment_indication(indication), .comment = context}; });
		};

		template<typename ParseState>
		using comment_hash_sign = comment<ParseState, make_comment_indication(ini::CommentIndication::HASH_SIGN)>;
		template<typename ParseState>
		using comment_semicolon = comment<ParseState, make_comment_indication(ini::CommentIndication::SEMICOLON)>;

		template<typename ParseState>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<ParseState>> |
				dsl::p<comment_semicolon<ParseState>>;

		template<typename ParseState>
		using comment_inline_hash_sign = comment_inline<ParseState, make_comment_indication(ini::CommentIndication::HASH_SIGN)>;
		template<typename ParseState>
		using comment_inline_semicolon = comment_inline<ParseState, make_comment_indication(ini::CommentIndication::SEMICOLON)>;

		template<typename ParseState>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<ParseState>> |
				dsl::p<comment_inline_semicolon<ParseState>>;

		struct group_name
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group name]"; }

			constexpr static auto				rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
							dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

			constexpr static auto value =// lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type
							{ return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

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

			constexpr static auto value =// lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type
							{ return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

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

			constexpr static auto value =// lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type
							{ return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		// identifier = [variable]
		template<typename ParseState>
		struct variable_declaration
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

			constexpr static auto				rule =
					LEXY_DEBUG("parse variable_declaration begin") +
					dsl::p<variable_key> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable_value>) +
					dsl::opt(comment_inline_production<ParseState>) +
					LEXY_DEBUG("parse variable_declaration end");

			constexpr static auto value = callback<void>(
					// blank line
					[]([[maybe_unused]] ParseState& state) {},
					// [identifier] = [variable] [comment]
					[](ParseState& state, const ini::string_view_type key, [[maybe_unused]] const ini::string_view_type value, const ini::comment_view_type comment) -> void
					{ state.value(key, comment); },
					// [identifier] = [] [variable]
					[](ParseState& state, const ini::string_view_type key, [[maybe_unused]] const ini::string_view_type value, lexy::nullopt) -> void
					{ state.value(key); },
					// [identifier] = [] [comment]
					[](ParseState& state, const ini::string_view_type key, lexy::nullopt, const ini::comment_view_type comment) -> void
					{ state.value(key, comment); },
					// [identifier] = [] []
					[](ParseState& state, const ini::string_view_type key, lexy::nullopt, lexy::nullopt) -> void
					{ state.value(key); });
		};

		template<typename ParseState>
		struct blank_line
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[empty line]"; }

			constexpr static auto				rule = dsl::peek(dsl::newline | dsl::unicode::blank) >>
										 (LEXY_DEBUG("empty line") +
										  dsl::until(dsl::newline).or_eof());

			constexpr static auto value = callback<void>(
					[](ParseState& state) -> void
					{ state.blank_line(); });
		};

		template<typename ParseState>
		struct variable_or_comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[variable or comment]"; }

			constexpr static auto				rule =
					// blank line
					dsl::p<blank_line<ParseState>> |
					// comment
					// todo: sign?
					(dsl::peek(
							 dsl::lit_c<comment_hash_sign<ParseState>::indication> |
							 dsl::lit_c<comment_semicolon<ParseState>::indication>) >>
					 comment_production<ParseState>) |
					// variable
					(dsl::else_ >>
					 (dsl::p<variable_declaration<ParseState>> +
					  // a newline required
					  dsl::newline));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename ParseState>
		struct group_declaration
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group declaration]"; }

			struct header : lexy::transparent_production
			{
				[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group head]"; }

				constexpr static auto				rule =
						LEXY_DEBUG("parse group_name begin") +
						// group name
						dsl::p<group_name> +
						LEXY_DEBUG("parse group_name end") +
						// ]
						dsl::square_bracketed.close() +
						dsl::opt(comment_inline_production<ParseState>) +
						dsl::until(dsl::newline);

				constexpr static auto value = callback<void>(
						// [group_name] [comment]
						[](ParseState& state, const ini::string_view_type group_name, const ini::comment_view_type comment) -> void
						{ state.begin_group(group_name, comment); },
						// [group_name] []
						[](ParseState& state, const ini::string_view_type group_name, lexy::nullopt) -> void
						{ state.begin_group(group_name); });
			};

			// end with 'eof' or next '[' (group begin)
			constexpr static auto rule =
					dsl::if_(comment_production<ParseState>) +
					// [
					(dsl::square_bracketed.open() >>
					 (dsl::p<header> +
					  LEXY_DEBUG("parse group properties begin") +
					  dsl::terminator(
							  dsl::eof |
							  dsl::peek(dsl::square_bracketed.open()))
							  .opt_list(
									  dsl::try_(
											  dsl::p<variable_or_comment<ParseState>>,
											  // ignore this line if an error raised
											  dsl::until(dsl::newline))) +
					  LEXY_DEBUG("parse group properties end")));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename ParseState>
		struct file
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[file context]"; }

			constexpr static auto				whitespace = dsl::ascii::blank;

			constexpr static auto				rule	   = dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<ParseState>>);

			constexpr static auto				value	   = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename T>
	struct is_flusher_state : std::false_type
	{
	};
	template<bool B1, bool B2>
	struct is_flusher_state<FlusherState<B1, B2>> : std::true_type
	{
	};
	template<typename T>
	struct is_flusher_state_with_comment : std::false_type
	{
	};
	template<bool B>
	struct is_flusher_state_with_comment<FlusherStateWithComment<B>> : std::true_type
	{
	};

	template<typename State>
		requires is_flusher_state<State>::value || is_flusher_state_with_comment<State>::value
	auto flush(State& state, const lexy::buffer<default_encoding>& buffer, const ini::string_view_type file_path) -> void
	{
		if (const auto result =
					lexy::parse<grammar::file<State>>(
							buffer,
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_path.data()));
			!result.has_value())
		{
			// todo: error ?
		}

#ifdef GAL_INI_TRACE_PARSE
		lexy::trace<grammar::file<State>>(
				stderr,
				buffer,
				{.flags = lexy::visualize_fancy});
#endif
	}
}// namespace

namespace gal::ini::impl
{
	template<typename Flusher, typename State>
	auto do_flush_override(typename Flusher::file_path_type file_path, const typename Flusher::context_type& context) -> FileFlushResult
	{
		std::filesystem::path path{file_path};
		if (!exists(path))
		{
			return FileFlushResult::FILE_NOT_FOUND;
		}

		if (auto file = lexy::read_file<default_encoding>(file_path.data());
			!file)
		{
			switch (file.error())
			{
				case lexy::file_error::file_not_found:
				{
					return FileFlushResult::FILE_NOT_FOUND;
				}
				case lexy::file_error::permission_denied:
				{
					return FileFlushResult::PERMISSION_DENIED;
				}
				case lexy::file_error::os_error:
				{
					return FileFlushResult::INTERNAL_ERROR;
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
			State state{file_path, context};

			flush(state, file.buffer(), file_path);

			return FileFlushResult::SUCCESS;
		}
	}

	auto IniFlusher::flush_override(IniExtractor::file_path_type file_path, bool keep_comments, bool keep_empty_group) const -> FileFlushResult
	{
		if (keep_comments)
		{
			if (keep_empty_group)
			{
				return do_flush_override<IniFlusher, FlusherState<true, true>>(file_path, context_);
			}
			return do_flush_override<IniFlusher, FlusherState<true, false>>(file_path, context_);
		}

		if (keep_empty_group)
		{
			return do_flush_override<IniFlusher, FlusherState<false, true>>(file_path, context_);
		}
		return do_flush_override<IniFlusher, FlusherState<false, false>>(file_path, context_);
	}

	auto IniFlusherWithComment::flush_override(file_path_type file_path, bool keep_empty_group) const -> FileFlushResult
	{
		if (keep_empty_group)
		{
			return do_flush_override<IniFlusherWithComment, FlusherStateWithComment<true>>(file_path, context_);
		}
		return do_flush_override<IniFlusherWithComment, FlusherStateWithComment<true>>(file_path, context_);
	}
}// namespace gal::ini::impl
