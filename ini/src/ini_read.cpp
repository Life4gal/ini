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

//#define GAL_INI_TRACE_PARSE

namespace
{
	namespace ini = gal::ini;

	template<typename StringType>
	[[nodiscard]] auto to_char_string(const StringType& string) -> decltype(auto)
	{
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

	using default_encoding = lexy::utf8_encoding;

	class Buffer
	{
	public:
		using encoding = default_encoding;
		using buffer_type = lexy::buffer<encoding>;
		using char_type = buffer_type::char_type;

	private:
		const buffer_type&                       buffer_;
		lexy::input_location_anchor<buffer_type> buffer_anchor_;

		const ini::file_path_type& file_path_;

	public:
		auto report_duplicate_declaration(const char_type* position, const ini::string_view_type identifier, const std::string_view category) const -> void
		{
			const auto location = lexy::get_input_location(buffer_, position, buffer_anchor_);

			const auto                        out = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{buffer_, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(out,
										lexy_ext::diagnostic_kind::warning,
										[&](lexy::cfile_output_iterator, lexy::visualization_options)
										{
											(void)std::fprintf(stderr, "duplicate %s declaration named '%s', ignore it...", category.data(), to_char_string(identifier).data());
											return out;
										});

			if (!file_path_.empty()) { (void)writer.write_path(out, file_path_.string().c_str()); }

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

		Buffer(
				const ini::file_path_type& file_path,
				const buffer_type&         buffer)
			: buffer_{buffer},
			buffer_anchor_{buffer_},
			file_path_{file_path} {}
	};

	template<typename Ini>
	class ParseState
	{
	public:
		using ini_type = Ini;
		using writer_type = typename ini_type::writer_type;

	private:
		Buffer buffer_;

		ini_type&                    ini_;
		std::unique_ptr<writer_type> writer_;

	public:
		ParseState(
				const Buffer::buffer_type& buffer,
				ini_type&                  ini)
			: buffer_{ini.file_path(), buffer},
			ini_{ini},
			writer_{nullptr} {}

		auto begin_group(const Buffer::char_type* position, ini::string_type&& group_name) -> void
		{
			writer_ = std::make_unique<writer_type>(ini_.write(std::move(group_name)));

			if (!writer_->empty())
			{
				// If we get here, it means that a group with the same name already exists before, then this 'group_name' will not be consumed because of move.
				buffer_.report_duplicate_declaration(position, writer_->name(), "group");
			}
		}

		auto value(const Buffer::char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
		{
			// Our parse ensures the writer is valid
			const auto& [inserted, result_key, result_value] = writer_->try_insert(std::move(key), std::move(value));
			if (!inserted)
			{
				// If we get here, it means that a key with the same name already exists before, then this 'key' will not be consumed because of move.
				buffer_.report_duplicate_declaration(position, result_key, "variable");
			}
		}
	};

	template<typename Ini>
	class ParseStateWithComment
	{
	public:
		using ini_type = Ini;
		using writer_type = typename ini_type::writer_type;

	private:
		Buffer buffer_;

		ini_type&                    ini_;
		std::unique_ptr<writer_type> writer_;

		ini::comment_type comment_;

	public:
		ParseStateWithComment(
				const Buffer::buffer_type& buffer,
				ini_type&                  ini)
			: buffer_{ini.file_path(), buffer},
			ini_{ini},
			writer_{nullptr},
			comment_{'\0', {}} {}

		auto comment(ini::comment_type&& comment) -> void { comment_ = std::move(comment); }

		auto begin_group(const Buffer::char_type* position, ini::string_type&& group_name, ini::comment_type&& inline_comment = {}) -> void
		{
			writer_ = std::make_unique<writer_type>(ini_.write(std::move(group_name)));

			if (!writer_->empty()) { buffer_.report_duplicate_declaration(position, writer_->name(), "group"); }

			writer_->comment(std::exchange(comment_, {}));
			writer_->inline_comment(std::move(inline_comment));
		}

		auto value(const Buffer::char_type* position, ini::string_type&& key, ini::string_type&& value, ini::comment_type&& inline_comment = {}) -> void
		{
			// Our parse ensures the writer is valid
			const auto& [inserted,
				result_comment,
				result_key,
				result_value,
				result_inline_comment] = writer_->try_insert(std::move(key),
															std::move(value),
															std::exchange(comment_, {}),
															std::move(inline_comment));
			if (!inserted) { buffer_.report_duplicate_declaration(position, result_key, "variable"); }
		}
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		template<bool Required>
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

			constexpr static auto value = []
			{
				if constexpr (Required) { return lexy::as_string<ini::string_type, default_encoding>; }
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename ParseState, bool Required, char Indication>
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
					dsl::p<comment_context<Required>> +
					LEXY_DEBUG("parse comment end") +
					dsl::newline);

			constexpr static auto value = []
			{
				if constexpr (Required)
				{
					return callback<void>(
							[](ParseState& state, ini::string_type&& context) -> void { state.comment({indication, std::move(context)}); });
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename ParseState, bool Required, char Indication>
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
					dsl::p<comment_context<Required>> +
					LEXY_DEBUG("parse inline_comment end"));

			constexpr static auto value = []
			{
				if constexpr (Required)
				{
					return callback<ini::comment_type>(
							[]([[maybe_unused]] ParseState& state, ini::string_type&& context) -> ini::comment_type { return {indication, std::move(context)}; });
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename ParseState, bool Required>
		using comment_hash_sign = comment<ParseState, Required, '#'>;
		template<typename ParseState, bool Required>
		using comment_semicolon = comment<ParseState, Required, ';'>;

		template<typename ParseState, bool Required>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<ParseState, Required>> |
				dsl::p<comment_semicolon<ParseState, Required>>;

		template<typename ParseState, bool Required>
		using comment_inline_hash_sign = comment_inline<ParseState, Required, '#'>;
		template<typename ParseState, bool Required>
		using comment_inline_semicolon = comment_inline<ParseState, Required, ';'>;

		template<typename ParseState, bool Required>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<ParseState, Required>> |
				dsl::p<comment_inline_semicolon<ParseState, Required>>;

		struct group_name
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group name]"; }

			constexpr static auto rule =
					dsl::identifier(
							// begin with printable
							dsl::unicode::print,
							// continue with printable, but excluding '\r', '\n', '\r\n' and ']'
							dsl::unicode::print - dsl::unicode::newline - dsl::square_bracketed.close());

			constexpr static auto value = lexy::as_string<ini::string_type, default_encoding>;
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

			constexpr static auto value = lexy::as_string<ini::string_type, default_encoding>;
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

			constexpr static auto value = lexy::as_string<ini::string_type, default_encoding>;
		};

		// identifier = [variable]
		template<typename ParseState, bool CommentRequired>
		struct variable_declaration
		{
		private:
			template<typename P, bool>
			struct value_generator;

			template<typename P>
			struct value_generator<P, false>
			{
				constexpr static auto value = callback<void>(
						// blank line
						[]([[maybe_unused]] ParseState& state) {},
						// [identifier] = [variable]
						[]<typename... Ignore>(ParseState& state, const Buffer::char_type* position, ini::string_type&& key, ini::string_type&& value, [[maybe_unused]] Ignore&&... ignore) -> void { state.value(position, std::move(key), std::move(value)); },
						// [identifier] = []
						[]<typename... Ignore>(ParseState& state, const Buffer::char_type* position, ini::string_type&& key, lexy::nullopt, [[maybe_unused]] Ignore&&... ignore) -> void { state.value(position, std::move(key), ini::string_type{}); });
			};

			template<typename P>
			struct value_generator<P, true>
			{
				constexpr static auto value = callback<void>(
						// blank line
						[]([[maybe_unused]] ParseState& state) {},
						// [identifier] = [variable] [comment]
						[](ParseState& state, const Buffer::char_type* position, ini::string_type&& key, ini::string_type&& value, ini::comment_type&& comment) -> void { state.value(position, std::move(key), std::move(value), std::move(comment)); },
						// [identifier] = [] [variable]
						[](ParseState& state, const Buffer::char_type* position, ini::string_type&& key, ini::string_type&& value, lexy::nullopt) -> void { state.value(position, std::move(key), std::move(value)); },
						// [identifier] = [] [comment]
						[](ParseState& state, const Buffer::char_type* position, ini::string_type&& key, lexy::nullopt, ini::comment_type&& comment) -> void { state.value(position, std::move(key), ini::string_type{}, std::move(comment)); },
						// [identifier] = [] []
						[](ParseState& state, const Buffer::char_type* position, ini::string_type&& key, lexy::nullopt, lexy::nullopt) -> void { state.value(position, std::move(key), ini::string_type{}); });
			};

		public:
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

			constexpr static auto rule =
					LEXY_DEBUG("parse variable_declaration begin") +
					dsl::position +
					dsl::p<variable_key> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable_value>) +
					dsl::opt(comment_inline_production<ParseState, CommentRequired>) +
					LEXY_DEBUG("parse variable_declaration end");

			constexpr static auto value = value_generator<ParseState, CommentRequired>::value;
		};

		template<typename ParseState, bool CommentRequired>
		struct variable_or_comment
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[variable or comment]"; }

			constexpr static auto rule =
					// ignore blank line
					(dsl::peek(dsl::newline | dsl::unicode::blank) >>
					(LEXY_DEBUG("ignore empty line") +
					dsl::until(dsl::newline).or_eof())) |
					// comment
					// todo: sign?
					(dsl::peek(
							dsl::lit_c<comment_hash_sign<ParseState, CommentRequired>::indication> |
							dsl::lit_c<comment_semicolon<ParseState, CommentRequired>::indication>) >>
					comment_production<ParseState, CommentRequired>) |
					// variable
					(dsl::else_ >>
					(dsl::p<variable_declaration<ParseState, CommentRequired>> +
					// a newline required
					dsl::newline));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename ParseState, bool CommentRequired>
		struct group_declaration
		{
		private:
			template<typename P, bool>
			struct value_generator;

			template<typename P>
			struct value_generator<P, false>
			{
				constexpr static auto             value = callback<void>(
						[]<typename... Ignore>(P& state, const Buffer::char_type* position, ini::string_type&& group_name, [[maybe_unused]] Ignore&&... ignore) -> void { state.begin_group(position, std::move(group_name)); });
			};

			template<typename P>
			struct value_generator<P, true>
			{
				constexpr static auto value = callback<void>(
						// [group_name] [comment]
						[](P& state, const Buffer::char_type* position, ini::string_type&& group_name, ini::comment_type&& comment) -> void { state.begin_group(position, std::move(group_name), std::move(comment)); },
						// [group_name] []
						[](P& state, const Buffer::char_type* position, ini::string_type&& group_name, lexy::nullopt) -> void { state.begin_group(position, std::move(group_name)); });
			};

		public:
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group declaration]"; }

			struct header : lexy::transparent_production
			{
				[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group head]"; }

				constexpr static auto rule =
						LEXY_DEBUG("parse group_name begin") +
						dsl::position +
						// group name
						dsl::p<group_name> +
						LEXY_DEBUG("parse group_name end") +
						// ]
						dsl::square_bracketed.close() +
						dsl::opt(comment_inline_production<ParseState, CommentRequired>) +
						dsl::until(dsl::newline);

				constexpr static auto value = value_generator<ParseState, CommentRequired>::value;
			};

			// end with 'eof' or next '[' (group begin)
			constexpr static auto rule =
					dsl::if_(comment_production<ParseState, CommentRequired>) +
					// [
					(dsl::square_bracketed.open() >>
					(dsl::p<header> +
					LEXY_DEBUG("parse group properties begin") +
					dsl::terminator(
							dsl::eof |
							dsl::peek(dsl::square_bracketed.open()))
					.opt_list(
							dsl::try_(
									dsl::p<variable_or_comment<ParseState, CommentRequired>>,
									// ignore this line if an error raised
									dsl::until(dsl::newline))) +
					LEXY_DEBUG("parse group properties end")));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename ParseState, bool CommentRequired>
		struct file
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[file context]"; }

			constexpr static auto whitespace = dsl::ascii::blank;

			constexpr static auto rule = dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<ParseState, CommentRequired>>);

			constexpr static auto value = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename ParseState, bool CommentRequired, typename Ini>
	auto parse(Ini& ini) -> void
	{
		const auto& file_path   = ini.file_path();
		const auto& file_string = file_path.string();

		// todo: encoding?
		if (auto file = lexy::read_file<default_encoding>(file_string.c_str()))
		{
			ParseState state{file.buffer(), ini};

			if (const auto result =
						lexy::parse<grammar::file<ParseState, CommentRequired>>(
								file.buffer(),
								state,
								lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(file_string.c_str()));
				!result.has_value())
			{
				// todo: error ?
			}

			#ifdef GAL_INI_TRACE_PARSE
			lexy::trace<grammar::file<ParseState, CommentRequired>>(
					stderr,
					file.buffer(),
					{.flags = lexy::visualize_fancy});
			#endif
		}
	}
}// namespace

namespace gal::ini::impl
{
	IniParser::IniParser(file_path_type&& file_path)
		: file_path_{std::move(file_path)} { parse<ParseState<IniParser>, false>(*this); }

	IniParserWithComment::IniParserWithComment(file_path_type&& file_path)
		: file_path_{std::move(file_path)} { parse<ParseStateWithComment<IniParserWithComment>, true>(*this); }
}// namespace gal::ini::impl
