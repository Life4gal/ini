#include <algorithm>
#include <filesystem>
#include <functional>
#include <ini/extractor.hpp>
#include <lexy/action/parse.hpp>
#include <lexy/action/trace.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/file.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <memory>
#include <utility>

//#define GAL_INI_TRACE_EXTRACT

#ifdef GAL_INI_TRACE_EXTRACT
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

	template<typename Encoding>
	struct buffer_descriptor
	{
		// does not own any of data
		using buffer_view_type		= lexy::string_input<Encoding>;
		using buffer_anchor_type	= lexy::input_location_anchor<buffer_view_type>;
		using default_encoding		= typename buffer_view_type::encoding;
		using default_char_type		= typename buffer_view_type::char_type;
		using default_position_type = const default_char_type*;
		static_assert(std::is_same_v<typename buffer_anchor_type::iterator, default_position_type>);

		buffer_view_type   buffer;
		buffer_anchor_type anchor;
		file_path_type	   file_path;
	};

	class ErrorReporter
	{
	public:
		constexpr static file_path_type buffer_file_path{"anonymous-buffer"};

		template<typename Descriptor, typename StringType>
		static auto report_duplicate_declaration(
				const Descriptor&								 descriptor,
				const typename Descriptor::default_position_type position,
				const StringType&								 identifier,
				const lexy_ext::diagnostic_kind					 kind,
				const std::string_view							 category,
				const std::string_view							 what_to_do) -> void
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

	template<typename Char, typename GroupAppend, typename KvAppend, typename Encoding>
	class ExtractorState
	{
	public:
		using group_append_type		 = GroupAppend;
		using kv_append_type		 = KvAppend;
		using encoding				 = Encoding;

		using string_view_type		 = std::basic_string_view<Char>;
		// todo: for comment
		using string_type			 = std::basic_string<Char>;

		using buffer_descriptor_type = buffer_descriptor<encoding>;
		using char_type				 = typename buffer_descriptor_type::default_char_type;
		using position_type			 = typename buffer_descriptor_type::default_position_type;

	private:
		buffer_descriptor_type descriptor_;

		group_append_type	   group_appender_;
		kv_append_type		   kv_appender_;

	public:
		ExtractorState(
				typename buffer_descriptor_type::buffer_view_type buffer,
				const file_path_type							  file_path,
				group_append_type								  group_appender)
			: descriptor_{buffer, typename buffer_descriptor_type::buffer_anchor_type{buffer}, file_path},
			  group_appender_{group_appender},
			  kv_appender_{} {}

		[[nodiscard]] auto buffer() const -> typename buffer_descriptor_type::buffer_view_type
		{
			return descriptor_.buffer;
		}

		[[nodiscard]] auto file_path() const -> file_path_type
		{
			return descriptor_.file_path;
		}

		auto group(const position_type position, const string_view_type group_name) -> void
		{
			const auto [name, kv_appender, inserted] = group_appender_(group_name);

			if (!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						name,
						lexy_ext::diagnostic_kind::note,
						"group",
						"subsequent elements are appended to the previously declared group");
			}

			kv_appender_ = kv_appender;
		}

		auto value(const position_type position, const string_view_type key, const string_view_type value) -> void
		{
			// Our parse ensures the kv_appender_ is valid
			if (
					const auto& [kv, inserted] = kv_appender_(key, value);
					!inserted)
			{
				ErrorReporter::report_duplicate_declaration(
						descriptor_,
						position,
						kv.first,
						lexy_ext::diagnostic_kind::warning,
						"variable",
						"this variable will be discarded");
			}
		}
	};

	namespace grammar
	{
		namespace dsl = lexy::dsl;

		template<typename State, bool Required>
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
				if constexpr (Required)
				{
					// todo
					// return lexy::as_string<typename State::string_type, typename State::encoding>;
					return lexy::noop;
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename State, bool Required, typename State::char_type Indication>
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
					 dsl::p<comment_context<State, Required>> +
					 LEXY_DEBUG("parse comment end") +
					 dsl::newline);

			constexpr static auto value = []
			{
				if constexpr (Required)
				{
					return callback<void>(
							[](State& state, typename State::string_type&& context) -> void
							{
								// state.comment({.indication = ini::make_comment_indication(indication), .comment = std::move(context)});
								(void)state;
								(void)context;
							});
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename State, bool Required, typename State::char_type Indication>
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
					 dsl::p<comment_context<State, Required>> +
					 LEXY_DEBUG("parse inline_comment end"));

			constexpr static auto value = []
			{
				if constexpr (Required)
				{
					// return callback<ini::comment_type>(
					// 		[]([[maybe_unused]] State& state, typename State::string_type&& context) -> ini::comment_type
					// 		{ return {.indication = ini::make_comment_indication(indication), .comment = std::move(context)}; });
					return lexy::noop;
				}
				else
				{
					// ignore it
					return lexy::noop;
				}
			}();
		};

		template<typename State, bool Required>
		using comment_hash_sign = comment<State, Required, ini::comment_indication_hash_sign<typename State::string_type>>;
		template<typename State, bool Required>
		using comment_semicolon = comment<State, Required, ini::comment_indication_semicolon<typename State::string_type>>;

		template<typename State, bool Required>
		constexpr auto comment_production =
				dsl::p<comment_hash_sign<State, Required>> |
				dsl::p<comment_semicolon<State, Required>>;

		template<typename State, bool Required>
		using comment_inline_hash_sign = comment_inline<State, Required, ini::comment_indication_hash_sign<typename State::string_type>>;
		template<typename State, bool Required>
		using comment_inline_semicolon = comment_inline<State, Required, ini::comment_indication_semicolon<typename State::string_type>>;

		template<typename State, bool Required>
		constexpr auto comment_inline_production =
				dsl::p<comment_inline_hash_sign<State, Required>> |
				dsl::p<comment_inline_semicolon<State, Required>>;

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

		// identifier = [variable]
		template<typename State, bool CommentRequired>
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
						[]([[maybe_unused]] State& state) {},
						// [identifier] = [variable]
						[]<typename... Ignore>(State& state, const typename State::position_type position, const typename State::string_view_type key, const typename State::string_view_type value, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.value(position, key, value); },
						// [identifier] = []
						[]<typename... Ignore>(State& state, const typename State::position_type position, const typename State::string_view_type key, lexy::nullopt, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.value(position, key, typename State::string_view_type{}); });
			};

			template<typename P>
			struct value_generator<P, true>
			{
				constexpr static auto value = callback<void>(
						// blank line
						[]([[maybe_unused]] State& state) {},
						// [identifier] = [variable] [comment]
						// [](State& state, const typename State::position_type position, const typename State::string_view_type key, const typename State::string_view_type value, ini::comment_type&& comment) -> void
						// { state.value(position, key, value, std::move(comment)); },
						// [identifier] = [] [variable]
						[](State& state, const typename State::position_type position, const typename State::string_view_type key, const typename State::string_view_type value, lexy::nullopt) -> void
						{ state.value(position, key, value); },
						// [identifier] = [] [comment]
						// [](State& state, const typename State::position_type position, const typename State::string_view_type key, lexy::nullopt, ini::comment_type&& comment) -> void
						// { state.value(position, key, typename State::string_view_type{}, std::move(comment)); },
						// [identifier] = [] []
						[](State& state, const typename State::position_type position, const typename State::string_view_type key, lexy::nullopt, lexy::nullopt) -> void
						{ state.value(position, key, typename State::string_view_type{}); });
			};

		public:
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[kv pair declaration]"; }

			constexpr static auto				rule =
					LEXY_DEBUG("parse variable_declaration begin") +
					dsl::position +
					dsl::p<variable_key<State>> +
					dsl::equal_sign +
					dsl::opt(dsl::p<variable_value<State>>) +
					dsl::opt(comment_inline_production<State, CommentRequired>) +
					LEXY_DEBUG("parse variable_declaration end");

			constexpr static auto value = value_generator<State, CommentRequired>::value;
		};

		template<typename State, bool CommentRequired>
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
							 dsl::lit_c<comment_hash_sign<State, CommentRequired>::indication> |
							 dsl::lit_c<comment_semicolon<State, CommentRequired>::indication>) >>
					 comment_production<State, CommentRequired>) |
					// variable
					(dsl::else_ >>
					 (dsl::p<variable_declaration<State, CommentRequired>> +
					  // a newline required
					  dsl::newline));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename State, bool CommentRequired>
		struct group_declaration
		{
		private:
			template<typename S, bool>
			struct value_generator;

			template<typename S>
			struct value_generator<S, false>
			{
				constexpr static auto value = callback<void>(
						[]<typename... Ignore>(S& state, const typename S::position_type position, const typename S::string_view_type group_name, [[maybe_unused]] Ignore&&... ignore) -> void
						{ state.group(position, group_name); });
			};

			template<typename S>
			struct value_generator<S, true>
			{
				constexpr static auto value = callback<void>(
						// [group_name] [comment]
						// [](P& state, const typename State::position_type position, const typename State::string_view_type group_name, ini::comment_type&& comment) -> void
						// { state.group(position, std::move(group_name), std::move(comment)); },
						// [group_name] []
						[](S& state, const typename S::position_type position, const typename S::string_view_type group_name, lexy::nullopt) -> void
						{ state.group(position, group_name); });
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
						dsl::p<group_name<State>> +
						LEXY_DEBUG("parse group_name end") +
						// ]
						dsl::square_bracketed.close() +
						dsl::opt(comment_inline_production<State, CommentRequired>) +
						dsl::until(dsl::newline);

				constexpr static auto value = value_generator<State, CommentRequired>::value;
			};

			// end with 'eof' or next '[' (group begin)
			constexpr static auto rule =
					dsl::if_(comment_production<State, CommentRequired>) +
					// [
					(dsl::square_bracketed.open() >>
					 (dsl::p<header> +
					  LEXY_DEBUG("parse group properties begin") +
					  dsl::terminator(
							  dsl::eof |
							  dsl::peek(dsl::square_bracketed.open()))
							  .opt_list(
									  dsl::try_(
											  dsl::p<variable_or_comment<State, CommentRequired>>,
											  // ignore this line if an error raised
											  dsl::until(dsl::newline))) +
					  LEXY_DEBUG("parse group properties end")));

			constexpr static auto value = lexy::forward<void>;
		};

		template<typename State, bool CommentRequired>
		struct file
		{
			[[nodiscard]] consteval static auto name() noexcept -> const char* { return "[file context]"; }

			constexpr static auto				whitespace = dsl::ascii::blank;

			constexpr static auto				rule =
					dsl::terminator(dsl::eof)
							.opt_list(
									dsl::try_(
											dsl::p<group_declaration<State, CommentRequired>>,
											// ignore this line if an error raised
											dsl::until(dsl::newline)));

			constexpr static auto value = lexy::forward<void>;
		};
	}// namespace grammar

	template<typename State>
	auto extract(State& state) -> void
	{
		// todo
		constexpr bool is_comment_required = false;

		if (const auto result =
					lexy::parse<grammar::file<State, is_comment_required>>(
							state.buffer(),
							state,
							lexy_ext::report_error.opts({.flags = lexy::visualize_fancy}).path(state.file_path().data()));
			!result.has_value())
		{
			// todo: error ?
		}

#ifdef GAL_INI_TRACE_EXTRACT
		lexy::trace<grammar::file<State, is_comment_required>>(
				stderr,
				state.buffer(),
				{.flags = lexy::visualize_fancy});
#endif
	}
}// namespace

namespace gal::ini::extractor_detail
{
	namespace
	{
		template<typename State>
		[[nodiscard]] auto extract_from_file(
				std::string_view							 file_path,
				group_append_type<typename State::char_type> group_appender) -> ExtractResult
		{
			std::filesystem::path path{file_path};
			if (!exists(path))
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

				extract(state);

				return ExtractResult::SUCCESS;
			}
		}

		template<typename State>
		[[nodiscard]] auto extract_from_buffer(
				std::basic_string_view<typename State::char_type> buffer,
				group_append_type<typename State::char_type>	  group_appender) -> ExtractResult
		{
			State state{{buffer.data(), buffer.size()}, ErrorReporter::buffer_file_path, group_appender};

			extract(state);

			return ExtractResult::SUCCESS;
		}
	}// namespace

	// char
	[[nodiscard]] auto extract_from_file(
			std::string_view		file_path,
			group_append_type<char> group_appender) -> ExtractResult
	{
		using char_type = char;
		// todo: encoding?
		using encoding	= lexy::utf8_char_encoding;

		return extract_from_file<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				file_path,
				group_appender);
	}

	// wchar_t
	//	 [[nodiscard]] auto extract_from_file(
	//	 		std::string_view		   file_path,
	//	 		group_append_type<wchar_t> group_appender) -> ExtractResult
	//	 {
	//	 	using char_type = wchar_t;
	//	 	// todo: encoding?
	//	 	using encoding	= lexy::utf32_encoding;
	//
	//	 	return extract_from_file<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
	//	 			file_path,
	//	 			group_appender);
	//	 }

	// char8_t
	[[nodiscard]] auto extract_from_file(
			std::string_view		   file_path,
			group_append_type<char8_t> group_appender) -> ExtractResult
	{
		using char_type = char8_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_file<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				file_path,
				group_appender);
	}

	// char16_t
	[[nodiscard]] auto extract_from_file(
			std::string_view			file_path,
			group_append_type<char16_t> group_appender) -> ExtractResult
	{
		using char_type = char16_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_file<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				file_path,
				group_appender);
	}

	// char32_t
	[[nodiscard]] auto extract_from_file(
			std::string_view			file_path,
			group_append_type<char32_t> group_appender) -> ExtractResult
	{
		using char_type = char32_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_file<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				file_path,
				group_appender);
	}

	// char
	[[nodiscard]] auto extract_from_buffer(
			std::basic_string_view<char> buffer,
			group_append_type<char>		 group_appender) -> ExtractResult
	{
		using char_type = char;
		// todo: encoding?
		using encoding	= lexy::utf8_char_encoding;

		return extract_from_buffer<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				buffer,
				group_appender);
	}

	// wchar_t
	//	[[nodiscard]] auto extract_from_buffer(
	//			std::basic_string_view<wchar_t> buffer,
	//			group_append_type<wchar_t>		group_appender) -> ExtractResult
	//	{
	//		using char_type = wchar_t;
	//		// todo: encoding?
	//		using encoding	= lexy::utf32_encoding;
	//
	//		return extract_from_buffer<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
	//				buffer,
	//				group_appender);
	//	}

	// char8_t
	[[nodiscard]] auto extract_from_buffer(
			std::basic_string_view<char8_t> buffer,
			group_append_type<char8_t>		group_appender) -> ExtractResult
	{
		using char_type = char8_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_buffer<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				buffer,
				group_appender);
	}

	// char16_t
	[[nodiscard]] auto extract_from_buffer(
			std::basic_string_view<char16_t> buffer,
			group_append_type<char16_t>		 group_appender) -> ExtractResult
	{
		using char_type = char16_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_buffer<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				buffer,
				group_appender);
	}

	// char32_t
	[[nodiscard]] auto extract_from_buffer(
			std::basic_string_view<char32_t> buffer,
			group_append_type<char32_t>		 group_appender) -> ExtractResult
	{
		using char_type = char32_t;
		using encoding	= lexy::deduce_encoding<char_type>;

		return extract_from_buffer<ExtractorState<char_type, group_append_type<char_type>, kv_append_type<char_type>, encoding>>(
				buffer,
				group_appender);
	}
}// namespace gal::ini::extractor_detail
