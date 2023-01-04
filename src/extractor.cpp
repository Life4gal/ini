#include <algorithm>
#include <filesystem>
#include <functional>
#include <ini/ini.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>

//#define GAL_INI_TRACE_PARSE

#ifdef GAL_INI_TRACE_PARSE
	#include <lexy/visualize.hpp>
#endif

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

	// does not own any of data
	using buffer_view_type		= lexy::string_input<lexy::utf8_encoding>;
	using buffer_anchor_type	= lexy::input_location_anchor<buffer_view_type>;
	using default_encoding		= buffer_view_type::encoding;
	using default_char_type		= buffer_view_type::char_type;
	using default_position_type = const default_char_type*;
	static_assert(std::is_same_v<buffer_anchor_type::iterator, default_position_type>);

	using file_path_type = ini::string_view_type;

	struct buffer_descriptor
	{
		buffer_view_type   buffer;
		buffer_anchor_type anchor;
		file_path_type	   file_path;
	};

	class ErrorReporter
	{
	public:
		constexpr static file_path_type buffer_file_path{"anonymous buffer"};

		static auto						report_duplicate_declaration(
									const buffer_descriptor&		descriptor,
									const default_char_type*		position,
									const ini::string_view_type		identifier,
									const lexy_ext::diagnostic_kind kind,
									const std::string_view			category,
									const std::string_view			what_to_do) -> void
		{
			const auto						  location = lexy::get_input_location(descriptor.buffer, position, descriptor.anchor);

			const auto						  out	   = lexy::cfile_output_iterator{stderr};
			const lexy_ext::diagnostic_writer writer{descriptor.buffer, {.flags = lexy::visualize_fancy}};

			(void)writer.write_message(
					out,
					kind,
					[&](lexy::cfile_output_iterator, lexy::visualization_options)
					{
						(void)std::fprintf(stderr, "duplicate %s declaration named '%s', %s...", category.data(), to_char_string(identifier).data(), what_to_do.data());
						return out;
					});

			if (!descriptor.file_path.empty()) { (void)writer.write_path(out, descriptor.file_path.data()); }

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

	class ExtractorState
	{
	public:
		using extractor_type = ini::impl::IniExtractor;
		using write_type	 = extractor_type::write_type;
		using group_type	 = extractor_type::group_type;
		using context_type	 = extractor_type::context_type;

	private:
		buffer_descriptor			descriptor_;

		context_type&				context_;
		std::unique_ptr<write_type> writer_;

	public:
		ExtractorState(
				const buffer_view_type buffer,
				const file_path_type   file_path,
				context_type&		   context)
			: descriptor_{.buffer = buffer, .anchor = buffer_anchor_type{buffer}, .file_path = file_path},
			  context_{context},
			  writer_{nullptr} {}

		[[nodiscard]] auto buffer() const -> buffer_view_type
		{
			return descriptor_.buffer;
		}

		[[nodiscard]] auto file_path() const -> file_path_type
		{
			return descriptor_.file_path;
		}

		auto begin_group(const default_char_type* position, ini::string_type&& group_name) -> void
		{
			const auto [it, inserted] = context_.try_emplace(std::move(group_name), group_type{});
			if (!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						it->first,
						lexy_ext::diagnostic_kind::note,
						"group",
						"subsequent elements are appended to the previously declared group");
			}

			writer_ = std::make_unique<write_type>(it->first, it->second);
		}

		auto value(const default_char_type* position, ini::string_type&& key, ini::string_type&& value) -> void
		{
			// Our parse ensures the writer is valid
			const auto& [inserted, result_key, result_value] = writer_->try_insert(std::move(key), std::move(value));
			if (!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						result_key,
						lexy_ext::diagnostic_kind::warning,
						"variable",
						"this variable will be discarded");
			}
		}
	};

	class ExtractorStateWithComment
	{
	public:
		using extractor_type = ini::impl::IniExtractorWithComment;
		using write_type	 = extractor_type::write_type;
		using group_type	 = extractor_type::group_type;
		using context_type	 = extractor_type::context_type;

	private:
		buffer_descriptor			descriptor_;

		context_type&				context_;
		std::unique_ptr<write_type> writer_;

		ini::comment_type			last_comment_;

	public:
		ExtractorStateWithComment(
				const buffer_view_type buffer,
				const file_path_type   file_path,
				context_type&		   context)
			: descriptor_{.buffer = buffer, .anchor = buffer_anchor_type{buffer}, .file_path = file_path},
			  context_{context},
			  writer_{nullptr},
			  last_comment_{ini::make_comment(ini::CommentIndication::INVALID, {})} {}

		[[nodiscard]] auto buffer() const -> buffer_view_type
		{
			return descriptor_.buffer;
		}

		[[nodiscard]] auto file_path() const -> file_path_type
		{
			return descriptor_.file_path;
		}

		auto comment(ini::comment_type&& comment) -> void { last_comment_ = std::move(comment); }

		auto begin_group(const default_char_type* position, ini::string_type&& group_name, ini::comment_type&& inline_comment = {}) -> void
		{
			const auto [it, inserted] = context_.try_emplace(std::move(group_name), group_type{});
			if (!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						it->first,
						lexy_ext::diagnostic_kind::note,
						"group",
						"subsequent elements are appended to the previously declared group");
			}

			writer_ = std::make_unique<write_type>(it->first, it->second);

			writer_->comment(std::exchange(last_comment_, {}));
			writer_->inline_comment(std::move(inline_comment));
		}

		auto value(const default_char_type* position, ini::string_type&& key, ini::string_type&& value, ini::comment_type&& inline_comment = {}) -> void
		{
			// Our parse ensures the writer is valid
			const auto& [inserted,
						 result_comment,
						 result_key,
						 result_value,
						 result_inline_comment] = writer_->try_insert(std::move(key),
																	  std::move(value),
																	  std::exchange(last_comment_, {}),
																	  std::move(inline_comment));
			if (!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						result_key,
						lexy_ext::diagnostic_kind::warning,
						"variable",
						"this variable will be discarded");
			}
		}
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		template<bool Required>
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

			constexpr static char				indication = Indication;

			constexpr static auto				rule =
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
							[](ParseState& state, ini::string_type&& context) -> void
							{ state.comment({.indication = ini::make_comment_indication(indication), .comment = std::move(context)}); });
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
			constexpr static char				indication = Indication;

			// constexpr static auto rule		 = comment<ParseState, Indication>::rule;
			constexpr static auto				rule =
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
							[]([[maybe_unused]] ParseState& state, ini::string_type&& context) -> ini::comment_type
							{ return {.indication = ini::make_comment_indication(indication), .comment = std::move(context)}; });
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename ParseState, bool Required>
		using comment_hash_sign = comment<ParseState, Required, make_comment_indication(ini::CommentIndication::HASH_SIGN)>;
		template<typename ParseState, bool Required>
		using comment_semicolon = comment<ParseState, Required, make_comment_indication(ini::CommentIndication::SEMICOLON)>;

		template<typename ParseState, bool Required>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<ParseState, Required>> |
				dsl::p<comment_semicolon<ParseState, Required>>;

		template<typename ParseState, bool Required>
		using comment_inline_hash_sign = comment_inline<ParseState, Required, make_comment_indication(ini::CommentIndication::HASH_SIGN)>;
		template<typename ParseState, bool Required>
		using comment_inline_semicolon = comment_inline<ParseState, Required, make_comment_indication(ini::CommentIndication::SEMICOLON)>;

		template<typename ParseState, bool Required>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<ParseState, Required>> |
				dsl::p<comment_inline_semicolon<ParseState, Required>>;

		struct group_name
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group name]"; }

			constexpr static auto				rule =
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
				constexpr auto begin_with_not_blank	   = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
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

			constexpr static auto				rule = []
			{
				// begin with not '\r', '\n', '\r\n', whitespace or '='
				constexpr auto begin_with_not_blank	   = dsl::unicode::print - dsl::unicode::newline - dsl::unicode::blank - dsl::equal_sign;
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
						[]<typename... Ignore>(ParseState& state, const default_char_type* position, ini::string_type&& key, ini::string_type&& value, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.value(position, std::move(key), std::move(value)); },
						// [identifier] = []
						[]<typename... Ignore>(ParseState& state, const default_char_type* position, ini::string_type&& key, lexy::nullopt, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.value(position, std::move(key), ini::string_type{}); });
			};

			template<typename P>
			struct value_generator<P, true>
			{
				constexpr static auto value = callback<void>(
						// blank line
						[]([[maybe_unused]] ParseState& state) {},
						// [identifier] = [variable] [comment]
						[](ParseState& state, const default_char_type* position, ini::string_type&& key, ini::string_type&& value, ini::comment_type&& comment) -> void
						{ state.value(position, std::move(key), std::move(value), std::move(comment)); },
						// [identifier] = [] [variable]
						[](ParseState& state, const default_char_type* position, ini::string_type&& key, ini::string_type&& value, lexy::nullopt) -> void
						{ state.value(position, std::move(key), std::move(value)); },
						// [identifier] = [] [comment]
						[](ParseState& state, const default_char_type* position, ini::string_type&& key, lexy::nullopt, ini::comment_type&& comment) -> void
						{ state.value(position, std::move(key), ini::string_type{}, std::move(comment)); },
						// [identifier] = [] []
						[](ParseState& state, const default_char_type* position, ini::string_type&& key, lexy::nullopt, lexy::nullopt) -> void
						{ state.value(position, std::move(key), ini::string_type{}); });
			};

		public:
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

			constexpr static auto				rule =
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

			constexpr static auto				rule =
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
				constexpr static auto value = callback<void>(
						[]<typename... Ignore>(P& state, const default_char_type* position, ini::string_type&& group_name, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.begin_group(position, std::move(group_name)); });
			};

			template<typename P>
			struct value_generator<P, true>
			{
				constexpr static auto value = callback<void>(
						// [group_name] [comment]
						[](P& state, const default_char_type* position, ini::string_type&& group_name, ini::comment_type&& comment) -> void
						{ state.begin_group(position, std::move(group_name), std::move(comment)); },
						// [group_name] []
						[](P& state, const default_char_type* position, ini::string_type&& group_name, lexy::nullopt) -> void
						{ state.begin_group(position, std::move(group_name)); });
			};

		public:
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group declaration]"; }

			struct header : lexy::transparent_production
			{
				[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[group head]"; }

				constexpr static auto				rule =
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

			constexpr static auto				whitespace = dsl::ascii::blank;

			constexpr static auto				rule	   = dsl::terminator(dsl::eof).opt_list(dsl::p<group_declaration<ParseState, CommentRequired>>);

			constexpr static auto				value	   = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename State>
		requires std::is_same_v<State, ExtractorState> || std::is_same_v<State, ExtractorStateWithComment>
	auto extract(State& state) -> void
	{
		constexpr bool is_comment_required = std::is_same_v<State, ExtractorStateWithComment>;

		if (const auto result =
					lexy::parse<grammar::file<State, is_comment_required>>(
							state.buffer(),
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(state.file_path().data()));
			!result.has_value())
		{
			// todo: error ?
		}

#ifdef GAL_INI_TRACE_PARSE
		lexy::trace<grammar::file<State, is_comment_required>>(
				stderr,
				state.buffer(),
				{.flags = lexy::visualize_fancy});
#endif
	}
}// namespace

namespace gal::ini::impl
{
	template<typename Extractor, typename State>
	auto do_extract_from_file(typename Extractor::file_path_type file_path, typename Extractor::context_type& out) -> FileReadResult
	{
		std::filesystem::path path{file_path};
		if (!exists(path))
		{
			return FileReadResult::FILE_NOT_FOUND;
		}

		if (auto file = lexy::read_file<default_encoding>(file_path.data());
			!file)
		{
			switch (file.error())
			{
				case lexy::file_error::file_not_found:
				{
					return FileReadResult::FILE_NOT_FOUND;
				}
				case lexy::file_error::permission_denied:
				{
					return FileReadResult::PERMISSION_DENIED;
				}
				case lexy::file_error::os_error:
				{
					return FileReadResult::INTERNAL_ERROR;
				}
				case lexy::file_error::_success:
				{
					GAL_INI_UNREACHABLE();
				}
			}
		}
		else
		{
			State state{{file.buffer().data(), file.buffer().size()}, file_path, out};

			extract(state);

			return FileReadResult::SUCCESS;
		}
	}

	template<typename Extractor, typename State>
	auto do_extract_from_buffer(typename Extractor::buffer_type string_buffer, typename Extractor::context_type& out) -> void
	{
		if (auto buffer = buffer_view_type{string_buffer};
			buffer.size())
		{
			State state{buffer, ErrorReporter::buffer_file_path, out};

			extract(state);
		}
	}


	auto IniExtractor::extract_from_file(file_path_type file_path, context_type& out) -> FileReadResult
	{
		return do_extract_from_file<IniExtractor, ExtractorState>(file_path, out);
	}

	auto IniExtractor::extract_from_file(file_path_type file_path) -> std::pair<FileReadResult, context_type>
	{
		context_type out{};
		auto		 result = extract_from_file(file_path, out);
		return {result, out};
	}

	auto IniExtractor::extract_from_buffer(buffer_type string_buffer, context_type& out) -> void
	{
		return do_extract_from_buffer<IniExtractor, ExtractorState>(string_buffer, out);
	}

	auto IniExtractor::extract_from_buffer(buffer_type string_buffer) -> context_type
	{
		context_type out;
		extract_from_buffer(string_buffer, out);
		return out;
	}

	auto IniExtractorWithComment::extract_from_file(file_path_type file_path, context_type& out) -> FileReadResult
	{
		return do_extract_from_file<IniExtractorWithComment, ExtractorStateWithComment>(file_path, out);
	}

	auto IniExtractorWithComment::extract_from_file(file_path_type file_path) -> std::pair<FileReadResult, context_type>
	{
		context_type out{};
		auto		 result = extract_from_file(file_path, out);
		return {result, out};
	}

	auto IniExtractorWithComment::extract_from_buffer(buffer_type string_buffer, context_type& out) -> void
	{
		return do_extract_from_buffer<IniExtractorWithComment, ExtractorStateWithComment>(string_buffer, out);
	}

	auto IniExtractorWithComment::extract_from_buffer(buffer_type string_buffer) -> context_type
	{
		context_type out;
		extract_from_buffer(string_buffer, out);
		return out;
	}
}// namespace gal::ini::impl
