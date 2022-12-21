#include <algorithm>
#include <filesystem>
#include <ini/ini.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/visualize.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>
#include <ranges>
#include <fstream>
#include <unordered_set>

//#define GAL_INI_TRACE_PARSE

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
	using reader_type = decltype(std::declval<const lexy::buffer<default_encoding>&>().reader());

	class Buffer
	{
	public:
		using path_type = std::filesystem::path;
		using out_type = std::ofstream;

	private:
		path_type file_path_;
		out_type  out_;

	public:
		explicit Buffer(const ini::file_path_type& filename)
			: file_path_{std::filesystem::temp_directory_path() / filename.filename()},
			out_{file_path_, std::ios::out | std::ios::trunc} { }

		[[nodiscard]] auto ready() const -> bool { return out_.is_open() && out_.good(); }

		auto writer() -> out_type& { return out_; }

		auto finish(const ini::file_path_type& filename) -> void
		{
			out_.close();

			std::filesystem::copy_file(
					file_path_,
					filename,
					std::filesystem::copy_options::overwrite_existing);
		}
	};
}

namespace gal::ini::impl
{
	template<typename Ini, bool KeepComments, bool KeepEmptyGroup>
	class FlushState
	{
	public:
		using ini_type = Ini;
		using flush_type = typename ini_type::flush_type;

		constexpr static bool keep_comments    = KeepComments;
		constexpr static bool keep_empty_group = KeepEmptyGroup;

	private:
		Buffer buffer_;

		ini_type&                   ini_;
		std::unique_ptr<flush_type> flusher_;
		comment_view_type           last_comment_;

		std::unordered_set<string_view_type, string_hash_type> pending_flushed_groups_;

		auto clear_last_comment() -> void { last_comment_ = {}; }

		auto flush_last_comment() -> void
		{
			if constexpr (keep_comments)
			{
				if (!last_comment_.empty())
				{
					buffer_.writer() << make_comment_indication(last_comment_.indication) << ' ' << last_comment_.comment << line_separator;
					clear_last_comment();
				}
			}
		}

		auto flush_group_head(const string_view_type name, const comment_view_type inline_comment = {}) -> void
		{
			flush_last_comment();

			buffer_.writer() << '[' << name << ']';
			if constexpr (keep_comments) { if (!inline_comment.empty()) { buffer_.writer() << ' ' << make_comment_indication(inline_comment.indication) << ' ' << inline_comment.comment; } }
			buffer_.writer() << line_separator;
		}

		auto flush_group_remainder() -> void { if (flusher_) { flusher_->flush_remainder(buffer_.writer()); } }

		auto flush_context_remainder() -> void
		{
			for (const auto& name: pending_flushed_groups_)
			{
				auto flusher = ini_.flush(name);

				buffer_.writer() << line_separator;
				flush_group_head(name);
				flusher.flush_remainder(buffer_.writer());
			}
		}

	public:
		explicit FlushState(ini_type& ini)
			: buffer_{ini.file_path()},
			ini_{ini},
			flusher_{nullptr},
			last_comment_{} { for (const auto& [name, _]: ini.context_) { pending_flushed_groups_.insert(name); } }

		FlushState(const FlushState&)                    = delete;
		auto operator=(const FlushState&) -> FlushState& = delete;
		FlushState(FlushState&&)                         = delete;
		auto operator=(FlushState&&) -> FlushState&      = delete;

		~FlushState() noexcept
		{
			flush_group_remainder();
			flush_context_remainder();
			buffer_.finish(ini_.file_path());
		}

		[[nodiscard]] auto ready() const -> bool { return buffer_.ready(); }

		auto comment(const comment_view_type comment) -> void { if constexpr (keep_comments) { last_comment_ = comment; } }

		auto begin_group(const string_view_type group_name, const comment_view_type inline_comment = {}) -> void
		{
			flush_group_remainder();

			flusher_ = std::make_unique<flush_type>(ini_.flush(group_name));
			pending_flushed_groups_.erase(group_name);

			if (flusher_->empty())
			{
				flusher_.reset();

				if constexpr (keep_empty_group) { flush_group_head(group_name, inline_comment); }
				else { clear_last_comment(); }
			}
			else { flush_group_head(group_name, inline_comment); }
		}

		auto value(const string_view_type key, const comment_view_type inline_comment = {}) -> void
		{
			if (flusher_ && flusher_->contains(key))
			{
				flush_last_comment();
				flusher_->flush(key, buffer_.writer());

				if constexpr (keep_comments) { if (!inline_comment.empty()) { buffer_.writer() << ' ' << make_comment_indication(inline_comment.indication) << ' ' << inline_comment.comment; } }
				buffer_.writer() << line_separator;
			}
			else { clear_last_comment(); }
		}

		auto blank_line() -> void { buffer_.writer() << line_separator; }
	};

	template<typename Ini, bool KeepEmptyGroup>
	class FlushStateWithComment
	{
	public:
		using ini_type = Ini;
		using flush_type = typename ini_type::flush_type;

		constexpr static bool keep_empty_group = KeepEmptyGroup;

	private:
		Buffer buffer_;

		ini_type&                   ini_;
		std::unique_ptr<flush_type> flusher_;

		std::unordered_set<string_view_type, string_hash_type> pending_flushed_groups_;

		auto flush_group_head(flush_type& flusher, const string_view_type name) -> void
		{
			if (flusher.has_comment())
			{
				const auto& comment = flusher.comment();
				buffer_.writer() << ini::make_comment_indication(comment.indication) << ' ' << comment.comment << line_separator;
			}

			buffer_.writer() << '[' << name << ']';
			if (flusher.has_inline_comment())
			{
				const auto& inline_comment = flusher.inline_comment();
				buffer_.writer() << ini::make_comment_indication(inline_comment.indication) << ' ' << inline_comment.comment;
			}
			buffer_.writer() << line_separator;
		}

		auto flush_group_remainder() -> void { if (flusher_) { flusher_->flush_remainder(buffer_.writer()); } }

		auto flush_context_remainder() -> void
		{
			for (const auto& name: pending_flushed_groups_)
			{
				auto flusher = ini_.flush(name);

				buffer_.writer() << line_separator;
				flush_group_head(flusher, name);
				flusher.flush_remainder(buffer_.writer());
			}
		}

	public:
		explicit FlushStateWithComment(ini_type& ini)
			: buffer_{ini.file_path()},
			ini_{ini},
			flusher_{nullptr} { for (const auto& [name, _]: ini.context_) { pending_flushed_groups_.insert(name); } }

		FlushStateWithComment(const FlushStateWithComment&)                    = delete;
		auto operator=(const FlushStateWithComment&) -> FlushStateWithComment& = delete;
		FlushStateWithComment(FlushStateWithComment&&)                         = delete;
		auto operator=(FlushStateWithComment&&) -> FlushStateWithComment&      = delete;

		~FlushStateWithComment() noexcept
		{
			flush_group_remainder();
			flush_context_remainder();
			buffer_.finish(ini_.file_path());
		}

		[[nodiscard]] auto ready() const -> bool { return buffer_.ready(); }

		auto comment([[maybe_unused]] const comment_view_type comment) const -> void { (void)this; }

		auto begin_group(const string_view_type group_name, [[maybe_unused]] const comment_view_type inline_comment = {}) -> void
		{
			flush_group_remainder();

			flusher_ = std::make_unique<flush_type>(ini_.flush(group_name));
			pending_flushed_groups_.erase(group_name);

			if (flusher_->empty())
			{
				if constexpr (keep_empty_group) { flush_group_head(*flusher_, group_name); }

				flusher_.reset();
			}
			else { flush_group_head(*flusher_, group_name); }
		}

		auto value(const string_view_type key, [[maybe_unused]] const comment_view_type inline_comment = {}) -> void
		{
			if (!flusher_) { }
			else { flusher_->flush(key, buffer_.writer()); }
		}

		auto blank_line() -> void { buffer_.writer() << line_separator; }
	};
}

namespace
{
	namespace grammar
	{
		namespace dsl = lexy::dsl;

		struct comment_context
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[comment context]"; }

			constexpr static auto rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n' and '\r\n'
							// todo: multi-line comment
							dsl::unicode::print - dsl::unicode::newline);

			constexpr static auto value = // lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type { return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		template<typename ParseState, char Indication>
		struct comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[comment]"; }

			constexpr static char indication = Indication;

			constexpr static auto rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse comment begin") +
					dsl::p<comment_context> +
					LEXY_DEBUG("parse comment end") +
					dsl::newline);

			constexpr static auto  value = callback<void>(
					[](ParseState& state, const ini::string_view_type context) -> void { state.comment({.indication = ini::make_comment_indication(indication), .comment = std::move(context)}); });
		};

		template<typename ParseState, char Indication>
		struct comment_inline
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[inline comment]"; }

			// constexpr static char indication = comment<ParseState, Indication>::indication;
			constexpr static char indication = Indication;

			// constexpr static auto rule		 = comment<ParseState, Indication>::rule;
			constexpr static auto rule =
					// begin with hash_sign
					dsl::lit_c<indication>
					//
					>>
					(LEXY_DEBUG("parse inline_comment begin") +
					dsl::p<comment_context> +
					LEXY_DEBUG("parse inline_comment end"));

			constexpr static auto                   value = callback<ini::comment_view_type>(
					[]([[maybe_unused]] ParseState& state, const ini::string_view_type context) -> ini::comment_view_type { return {.indication = ini::make_comment_indication(indication), .comment = std::move(context)}; });
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

			constexpr static auto rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
							dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

			constexpr static auto value = // lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type { return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
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
				constexpr auto begin_with_not_blank = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
				// continue with printable, but excluding '\r', '\n', '\r\n', whitespace and '='
				constexpr auto continue_with_printable = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

				return dsl::peek(begin_with_not_blank) >> dsl::identifier(begin_with_not_blank, continue_with_printable) |
						// This error can make the line parsing fail immediately when the [key] cannot be parsed, and then skip this line (instead of trying to make other possible matches).
						dsl::error<invalid_key>;
			}();

			constexpr static auto value = // lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type { return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		struct variable_value
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[value]"; }

			constexpr static auto rule = []
			{
				// begin with not '\r', '\n', '\r\n', whitespace or '='
				constexpr auto begin_with_not_blank = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
				// continue with printable, but excluding '\r', '\n', '\r\n', whitespace and '='
				constexpr auto continue_with_printable = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;

				return dsl::peek(begin_with_not_blank) >> dsl::identifier(begin_with_not_blank, continue_with_printable);
			}();

			constexpr static auto value = // lexy::as_string<ini::string_view_type, default_encoding>;
					lexy::callback<ini::string_view_type>(
							[](const lexy::lexeme<reader_type> lexeme) -> ini::string_view_type { return {reinterpret_cast<const ini::char_type*>(lexeme.data()), lexeme.size()}; });
		};

		// identifier = [variable]
		template<typename ParseState>
		struct variable_declaration
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

			constexpr static auto rule =
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
					[](ParseState& state, const ini::string_view_type key, [[maybe_unused]] const ini::string_view_type value, const ini::comment_view_type comment) -> void { state.value(key, comment); },
					// [identifier] = [] [variable]
					[](ParseState& state, const ini::string_view_type key, [[maybe_unused]] const ini::string_view_type value, lexy::nullopt) -> void { state.value(key); },
					// [identifier] = [] [comment]
					[](ParseState& state, const ini::string_view_type key, lexy::nullopt, const ini::comment_view_type comment) -> void { state.value(key, comment); },
					// [identifier] = [] []
					[](ParseState& state, const ini::string_view_type key, lexy::nullopt, lexy::nullopt) -> void { state.value(key); });
		};

		template<typename ParseState>
		struct blank_line
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[empty line]"; }

			constexpr static auto rule = dsl::peek(dsl::newline | dsl::unicode::blank) >>
										(LEXY_DEBUG("empty line") +
										dsl::until(dsl::newline).or_eof());

			constexpr static auto  value = callback<void>(
					[](ParseState& state) -> void { state.blank_line(); });
		};

		template<typename ParseState>
		struct variable_or_comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[variable or comment]"; }

			constexpr static auto rule =
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

				constexpr static auto rule =
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
						[](ParseState& state, const ini::string_view_type group_name, const ini::comment_view_type comment) -> void { state.begin_group(group_name, comment); },
						// [group_name] []
						[](ParseState& state, const ini::string_view_type group_name, lexy::nullopt) -> void { state.begin_group(group_name); });
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

			constexpr static auto whitespace = dsl::ascii::blank;

			constexpr static auto rule = dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<ParseState>>);

			constexpr static auto value = lexy::forward<void>;
		};
	}

	template<typename FlushState, typename Ini>
	auto flush(Ini& ini) -> void
	{
		const auto& file_path   = ini.file_path();
		const auto& file_string = file_path.string();

		// todo: encoding?
		if (auto file = lexy::read_file<default_encoding>(file_string.c_str()))
		{
			FlushState state{ini};

			if (!state.ready())
			{
				// todo: error?
				return;
			}

			if (const auto result =
						lexy::parse<grammar::file<FlushState>>(
								file.buffer(),
								state,
								lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_string.c_str()));
				!result.has_value())
			{
				// todo: error ?
			}

			#ifdef GAL_INI_TRACE_PARSE
			lexy::trace<grammar::file<ParseState>>(
					stderr,
					file.buffer(),
					{.flags = lexy::visualize_fancy});
			#endif
		}
	}
}

namespace gal::ini::impl
{
	namespace detail
	{
		GroupAccessor<GroupProperty::WRITE_ONLY>::GroupAccessor(const GroupAccessor<GroupProperty::READ_ONLY>::group_type& group)
		{
			std::ranges::transform(
					group,
					std::inserter(group_, group_.end()),
					[](const auto& pair) -> group_type::value_type { return group_type::value_type{pair.first, pair.second}; }
					);
		}

		auto GroupAccessor<GroupProperty::WRITE_ONLY>::flush(const string_view_type key, std::ostream& out) -> void
		{
			if (const auto it = group_.find(key);
				it != group_.end())
			{
				out << it->first << '=' << it->second;
				group_.erase(it);
			}
		}

		auto GroupAccessor<GroupProperty::WRITE_ONLY>::flush_remainder(std::ostream& out) -> void
		{
			for (const auto& [key, value]: group_) { out << key << '=' << value << line_separator; }
			group_.clear();
		}

		GroupAccessor<GroupProperty::WRITE_ONLY_WITH_COMMENT>::GroupAccessor(const GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>::group_with_comment_type& group)
		{
			std::ranges::transform(
					group.group,
					std::inserter(group_.group, group_.group.end()),
					[](const auto& pair) -> group_type::value_type { return group_type::value_type{pair.first, variable_with_comment{.comment = pair.second.comment, .variable = pair.second.variable, .inline_comment = pair.second.inline_comment}}; }
					);

			group_.comment        = group.comment;
			group_.inline_comment = group.inline_comment;
		}

		auto GroupAccessor<GroupProperty::WRITE_ONLY_WITH_COMMENT>::flush(const string_view_type key, std::ostream& out) -> void
		{
			if (const auto it = group_.group.find(key);
				it != group_.group.end())
			{
				if (!it->second.comment.empty())
				{
					const auto& [indication, comment] = it->second.comment;
					out << make_comment_indication(indication) << ' ' << comment << line_separator;
				}

				out << it->first << '=' << it->second.variable;
				if (!it->second.inline_comment.empty())
				{
					const auto& [indication, comment] = it->second.inline_comment;
					out << ' ' << make_comment_indication(indication) << ' ' << comment;
				}
				out << line_separator;

				group_.group.erase(it);
			}
		}

		auto GroupAccessor<GroupProperty::WRITE_ONLY_WITH_COMMENT>::flush_remainder(std::ostream& out) -> void
		{
			for (const auto& [key, variable_with_comment]: group_.group)
			{
				if (!variable_with_comment.comment.empty())
				{
					const auto& [indication, comment] = variable_with_comment.comment;
					out << make_comment_indication(indication) << ' ' << comment << line_separator;
				}

				out << key << '=' << variable_with_comment.variable;
				if (!variable_with_comment.inline_comment.empty())
				{
					const auto& [indication, comment] = variable_with_comment.inline_comment;
					out << ' ' << make_comment_indication(indication) << ' ' << comment;
				}
				out << line_separator;
			}
			group_.group.clear();
		}
	}// namespace detail

	auto IniParser::flush(const bool keep_comments, const bool keep_empty_group) -> void
	{
		if (keep_comments)
		{
			if (keep_empty_group) { ::flush<FlushState<IniParser, true, true>>(*this); }
			else { ::flush<FlushState<IniParser, true, false>>(*this); }
		}
		else
		{
			if (keep_empty_group) { ::flush<FlushState<IniParser, false, true>>(*this); }
			else { ::flush<FlushState<IniParser, false, false>>(*this); }
		}
	}

	auto IniParserWithComment::flush(bool keep_empty_group) -> void
	{
		if (keep_empty_group) { ::flush<FlushStateWithComment<IniParserWithComment, true>>(*this); }
		else { ::flush<FlushStateWithComment<IniParserWithComment, false>>(*this); }
	}
}
