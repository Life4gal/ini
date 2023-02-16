#pragma once

#include <ini/internal/common.hpp>
// for std::basic_ostream
#include <iosfwd>

namespace gal::ini
{
	enum class FlushResult
	{
		// The file was not found.
		// FILE_NOT_FOUND,
		// The file cannot be opened.
		PERMISSION_DENIED,
		// An internal OS error, such as failure to read from the file.
		INTERNAL_ERROR,

		SUCCESS,
	};

	// 1. std::basic_ostream
	template<typename Char>
	using ostream_type = std::basic_ostream<Char>;
	// 2. Inherit this class
	template<typename Char>
	class UserOut
	{
	public:
		using char_type																		  = Char;

		constexpr UserOut() noexcept														  = default;
		constexpr UserOut(const UserOut&) noexcept											  = default;
		constexpr UserOut(UserOut&&) noexcept												  = default;
		constexpr auto operator=(const UserOut&) noexcept -> UserOut&						  = default;
		constexpr auto operator=(UserOut&&) noexcept -> UserOut&							  = default;
		constexpr virtual ~UserOut() noexcept												  = default;

		constexpr virtual auto operator<<(char_type data) -> UserOut&						  = 0;
		constexpr virtual auto operator<<(std::basic_string_view<char_type> data) -> UserOut& = 0;
	};

	// Determines if a key exists in the current group.
	// note: This function is used to determine whether the comment and inline_comment of the key-value pair should be written to the file (for deleted key-value pairs, we discard the comment).
	template<typename Char>
	using kv_contains_type =
			StackFunction<
					auto
					// pass key
					(std::basic_string_view<Char> key)
							// return key exists
							->bool>;

	// We read a key from the file, and the user writes the key-value pair indicated by the target key to out (or not, the choice is up to the user).
	// note: We pass an output stream (usually an output file) to the user, and it is the user's responsibility to write all remaining key-value pairs "correctly" (although it is possible to add content, such as comments, while still ensuring correct formatting). We have to do this because we cannot assume the user's key-value pair type.
	// note: DO NOT write newlines unless you want the inline_comment (if it exists) to be written to the next line.
	template<typename Char>
	using kv_flush_ostream_type =
			StackFunction<
					auto
					// pass ostream and key
					(ostream_type<Char>& out, std::basic_string_view<Char> key)
							// return nothing
							->void>;

	template<typename Char>
	using kv_flush_user_type =
			StackFunction<
					auto
					// pass key
					(std::basic_string_view<Char> key)
							// return nothing
							->void>;

	// When we are done parsing a group, we need to add the remaining (newly added) key-value pairs to the back of the group as well.
	// note: This requires that the key-value pairs of a group not be separated, or that the same group not be declared twice (or more), otherwise we don't know when we need to flush.
	// note: We pass an output stream (usually an output file) to the user, and it is the user's responsibility to write all remaining key-value pairs "correctly" (although it is possible to add content, such as comments, while still ensuring correct formatting). We have to do this because we cannot assume the user's key-value pair type.
	template<typename Char>
	using kv_flush_remaining_ostream_type =
			StackFunction<
					auto
					// pass ostream
					(ostream_type<Char>& out)
							// return nothing
							->void>;

	template<typename Char>
	using kv_flush_remaining_user_type =
			StackFunction<
					auto()
							// return nothing
							->void>;

	template<typename Char>
	struct kv_ostream_handle
	{
		// group name
		std::basic_string_view<Char> name{};
		// kv contains handle
		kv_contains_type<Char>		 contains{
				  [](const auto&) -> bool
				  { return false; }};

		// kv flush handle
		kv_flush_ostream_type<Char> flush{
				[](const auto&, const auto&) -> void {}};

		// kv flush remaining handle
		kv_flush_remaining_ostream_type<Char> flush_remaining{
				[](const auto&) -> void {}};
	};

	template<typename Char>
	struct kv_user_handle
	{
		// group name
		std::basic_string_view<Char> name{};
		// kv contains handle
		kv_contains_type<Char>		 contains{
				  [](const auto&) -> bool
				  { return false; }};

		// kv flush handle
		kv_flush_user_type<Char> flush{
				[](const auto&) -> void {}};

		// kv flush remaining handle
		kv_flush_remaining_user_type<Char> flush_remaining{
				[]() -> void {}};
	};


	// Determines if a group exists in the current file.
	// note: This function is used to determine whether the comment and inline_comment of the group should be written to the file (for deleted groups, `we discard the comment` -- see below).
	// note: You can have this function return true, so that we keep the group (even though it doesn't contain any key-value pairs), and keep its comment.
	template<typename Char>
	using group_contains_type =
			StackFunction<
					auto
					// pass group name
					(std::basic_string_view<Char> group_name)
							// return group exists
							->bool>;

	// We read a group name from the file, and the user writes the group head indicated by the target group name to out (or not, the choice is up to the user).
	// note: We pass an output stream (usually an output file) to the user, and it is the user's responsibility to write group head "correctly" (although it is possible to add content, such as comments, while still ensuring correct formatting).
	// note: We could have written the required content ourselves (e.g., `[group_name]`), but we left that up to the user.
	// note: DO NOT write newlines unless you want the inline_comment (if it exists) to be written to the next line.
	template<typename Char>
	using group_flush_ostream_type =
			StackFunction<
					auto
					// pass ostream and group name
					(ostream_type<Char>& out, std::basic_string_view<Char> group_name)
							// return kv_handler
							->kv_ostream_handle<Char>>;

	template<typename Char>
	using group_flush_user_type =
			StackFunction<
					auto
					// pass group name
					(std::basic_string_view<Char> group_name)
							// return kv_handler
							->kv_user_handle<Char>>;

	// After we have parsed all the groups in the file, we need to add the remaining (newly added) groups to the back of the file as well.
	// note: We pass an output stream (usually an output file) to the user, and it is the user's responsibility to write all remaining groups and key-value pairs "correctly" (although it is possible to add content, such as comments, while still ensuring correct formatting). We have to do this because we cannot assume the user's group and key-value pair type.
	template<typename Char>
	using group_flush_remaining_ostream_type =
			StackFunction<
					auto
					// pass ostream
					(ostream_type<Char>& out)
							// return nothing
							->void>;

	template<typename Char>
	using group_flush_remaining_user_type =
			StackFunction<
					auto()
							// return nothing
							->void>;

	template<typename Char>
	struct group_ostream_handle
	{
		// group contains handle
		group_contains_type<Char> contains{
				[](const auto&) -> bool
				{ return false; }};

		// group flush handle
		group_flush_ostream_type<Char> flush{
				[](const auto&, const auto&) -> void {}};

		// group flush remaining handle
		group_flush_remaining_ostream_type<Char> flush_remaining{
				[](const auto&) -> void {}};
	};

	template<typename Char>
	struct group_user_handle
	{
		// get user out
		// note: no default definition given
		StackFunction<auto()->UserOut<Char>&> user{};

		// group contains handle
		group_contains_type<Char>			  contains{
				[](const auto&) -> bool
				{ return false; }};

		// group flush handle
		group_flush_user_type<Char> flush{
				[](const auto&) -> void {}};

		// group flush remaining handle
		group_flush_remaining_user_type<Char> flush_remaining{
				[]() -> void {}};
	};

	namespace flusher_detail
	{
		// ==============================================
		// This is a very bad design, we have to iterate through all possible character types,
		// even StackFunction is on the edge of UB,
		// but in order to minimize dependencies and allow the user to maximize customization of the type, this design seems to be the only option.
		// ==============================================

		// char
		[[nodiscard]] auto flush_to_file(
				std::string_view		   file_path,
				group_ostream_handle<char> group_handler) -> FlushResult;

		// wchar_t
		// [[nodiscard]] auto flush_to_file(
		// 		std::string_view		   file_path,
		// 		group_ostream_handler<wchar_t> group_handler) -> FlushResult;

		// char8_t
		[[nodiscard]] auto flush_to_file(
				std::string_view			  file_path,
				group_ostream_handle<char8_t> group_handler) -> FlushResult;

		// char16_t
		[[nodiscard]] auto flush_to_file(
				std::string_view			   file_path,
				group_ostream_handle<char16_t> group_handler) -> FlushResult;

		// char32_t
		[[nodiscard]] auto flush_to_file(
				std::string_view			   file_path,
				group_ostream_handle<char32_t> group_handler) -> FlushResult;

		// char
		[[nodiscard]] auto flush_to_user(
				std::string_view		file_path,
				group_user_handle<char> group_handler) -> FlushResult;

		// wchar_t
		// [[nodiscard]] auto flush_to_user(
		// 		std::string_view		   file_path,
		// 		group_user_handler<wchar_t> group_handler) -> FlushResult;

		// char8_t
		[[nodiscard]] auto flush_to_user(
				std::string_view		   file_path,
				group_user_handle<char8_t> group_handler) -> FlushResult;

		// char16_t
		[[nodiscard]] auto flush_to_user(
				std::string_view			file_path,
				group_user_handle<char16_t> group_handler) -> FlushResult;

		// char32_t
		[[nodiscard]] auto flush_to_user(
				std::string_view			file_path,
				group_user_handle<char32_t> group_handler) -> FlushResult;
	}// namespace flusher_detail

	/**
	 * @brief Flush ini data to files.
	 * @tparam ContextType Type of the input data.
	 * @param file_path The (absolute) path to the file.
	 * @param group_handler Group handler.
	 * @return Flush result.
	 */
	template<typename ContextType>
	auto flush_to_file(
			std::string_view																		 file_path,
			group_ostream_handle<typename string_view_t<typename ContextType::key_type>::value_type> group_handler) -> FlushResult
	{
		return flusher_detail::flush_to_file(
				file_path,
				group_handler);
	}

	template<typename ContextType>
	auto flush_to_user(
			std::string_view																	  file_path,
			group_user_handle<typename string_view_t<typename ContextType::key_type>::value_type> group_handler) -> FlushResult
	{
		return flusher_detail::flush_to_user(
				file_path,
				group_handler);
	}

	/**
	 * @brief Flush ini data from files.
	 * @tparam ContextType Type of the input data.
	 * @param file_path The (absolute) path to the file.
	 * @param in Where the extracted data is stored.
	 * @return Extract result.
	 */
	template<typename ContextType>
	auto flush_to_file(std::string_view file_path, ContextType& in) -> FlushResult
	{
		using context_type						  = ContextType;

		using key_type							  = typename context_type::key_type;
		using group_type						  = typename context_type::mapped_type;

		using group_key_type					  = typename group_type::key_type;
		using group_mapped_type					  = typename group_type::mapped_type;

		using char_type							  = typename string_view_t<key_type>::value_type;

		using group_view_type					  = common::map_type_t<context_type, string_view_t<key_type>, const group_type*>;
		using kv_view_type						  = common::map_type_t<group_type, string_view_t<group_key_type>, string_view_t<group_mapped_type>>;

		constexpr static auto do_flush_group_head = [](ostream_type<char_type>& out, const std::basic_string_view<char_type> group_name) -> void
		{
			// '[' group_name ']'
			// no '\n', see `group_flush_type`
			out
					<< square_bracket<key_type>.first
					<< group_name
					<< square_bracket<key_type>.second;
		};
		constexpr static auto do_flush_kv = [](ostream_type<char_type>& out, const std::basic_string_view<char_type> key, const std::basic_string_view<char_type> value) -> void
		{
			// key 'space' '=' 'space' value
			// no '\n', see `kv_flush_type`
			out
					<< key
					<< blank_separator<group_key_type> << kv_separator<group_key_type> << blank_separator<group_key_type> << value;
		};

		// We need the following two temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming StackFunction.

		// all group view
		auto group_view = [](const auto& gs) -> group_view_type
		{
			group_view_type vs{};
			for (const auto& g: gs)
			{
				vs.emplace(g.first, &g.second);
			}
			return vs;
		}(in);

		// current kvs view
		kv_view_type kv_view{};

		// !!!MUST PLACE HERE!!!
		// StackFunction keeps the address of the lambda and forwards the argument to the lambda when StackFunction::operator() has been called.
		// This requires that the lambda "must" exist at this point (i.e. have a longer lifecycle than the StackFunction), which is fine for a single-level lambda (maybe?).
		// However, if there is nesting, then the lambda will end its lifecycle early and the StackFunction will refer to an illegal address.
		// Walking on the edge of UB!
		auto		 kv_contains =
				[&kv_view](const string_view_t<group_key_type> key) -> bool
		{
			return kv_view.contains(key);
		};

		auto kv_flush =
				[&kv_view](ostream_type<char_type>& out, const std::basic_string_view<char_type> key) -> void
		{
			if (const auto kv_it = kv_view.find(key);
				kv_it != kv_view.end())
			{
				do_flush_kv(out, kv_it->first, kv_it->second);
				// remove this key
				kv_view.erase(kv_it);
			}

			// else, do nothing
		};

		auto kv_flush_remaining =
				[&kv_view](ostream_type<char_type>& out) -> void
		{
			for (const auto& kv: kv_view)
			{
				do_flush_kv(out, kv.first, kv.second);
				// note: newlines
				out << line_separator<std::basic_string_view<char_type>>;
			}
			// clear
			kv_view.clear();
		};

		return flush_to_file<ContextType>(
				file_path,
				group_ostream_handle<char_type>{
						.contains =
								group_contains_type<char_type>{
										[&group_view](string_view_t<key_type> group_name) -> bool
										{
											return group_view.contains(group_name);
										}},
						.flush =
								group_flush_ostream_type<char_type>{
										[&group_view, &kv_view, &kv_contains, &kv_flush, &kv_flush_remaining](ostream_type<char_type>& out, const std::basic_string_view<char_type> group_name) -> kv_ostream_handle<char_type>
										{
											if (const auto group_it = group_view.find(group_name);
												group_it != group_view.end())
											{
												// flush head
												do_flush_group_head(out, group_name);

												// set current kvs view
												for (const auto& kv: *group_it->second)
												{
													kv_view.emplace(kv.first, kv.second);
												}

												// remove this group from view
												group_view.erase(group_name);

												return {
														.name			 = group_name,
														.contains		 = kv_contains,
														.flush			 = kv_flush,
														.flush_remaining = kv_flush_remaining};
											}

											return {};
										}},
						.flush_remaining =
								group_flush_remaining_ostream_type<char_type>{
										[&group_view](ostream_type<char_type>& out) -> void
										{
											for (const auto& group: group_view)
											{
												// flush head
												do_flush_group_head(out, group.first);
												out << line_separator<key_type>;

												// kvs
												for (const auto& kv: *group.second)
												{
													do_flush_kv(out, kv.first, kv.second);
													out << line_separator<group_key_type>;
												}
											}

											// clear
											group_view.clear();
										}}});
	}

	template<typename ContextType>
	auto flush_to_user(std::string_view file_path, ContextType& in, UserOut<typename string_view_t<typename ContextType::key_type>::value_type>& user) -> FlushResult
	{
		using context_type						  = ContextType;

		using key_type							  = typename context_type::key_type;
		using group_type						  = typename context_type::mapped_type;

		using group_key_type					  = typename group_type::key_type;
		using group_mapped_type					  = typename group_type::mapped_type;

		using char_type							  = typename string_view_t<key_type>::value_type;

		using group_view_type					  = common::map_type_t<context_type, string_view_t<key_type>, const group_type*>;
		using kv_view_type						  = common::map_type_t<group_type, string_view_t<group_key_type>, string_view_t<group_mapped_type>>;

		constexpr static auto do_flush_group_head = [](UserOut<char_type>& out, const std::basic_string_view<char_type> group_name) -> void
		{
			// '[' group_name ']'
			// no '\n', see `group_flush_type`
			out
					<< square_bracket<key_type>.first
					<< group_name
					<< square_bracket<key_type>.second;
		};
		constexpr static auto do_flush_kv = [](UserOut<char_type>& out, const std::basic_string_view<char_type> key, const std::basic_string_view<char_type> value) -> void
		{
			// key 'space' '=' 'space' value
			// no '\n', see `kv_flush_type`
			out
					<< key
					<< blank_separator<group_key_type> << kv_separator<group_key_type> << blank_separator<group_key_type> << value;
		};

		// We need the following two temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming StackFunction.

		// all group view
		auto group_view = [](const auto& gs) -> group_view_type
		{
			group_view_type vs{};
			for (const auto& g: gs)
			{
				vs.emplace(g.first, &g.second);
			}
			return vs;
		}(in);

		// current kvs view
		kv_view_type kv_view{};

		// !!!MUST PLACE HERE!!!
		// StackFunction keeps the address of the lambda and forwards the argument to the lambda when StackFunction::operator() has been called.
		// This requires that the lambda "must" exist at this point (i.e. have a longer lifecycle than the StackFunction), which is fine for a single-level lambda (maybe?).
		// However, if there is nesting, then the lambda will end its lifecycle early and the StackFunction will refer to an illegal address.
		// Walking on the edge of UB!
		auto		 kv_contains =
				[&kv_view](const string_view_t<group_key_type> key) -> bool
		{
			return kv_view.contains(key);
		};

		auto kv_flush =
				[&user, &kv_view](const std::basic_string_view<char_type> key) -> void
		{
			if (const auto kv_it = kv_view.find(key);
				kv_it != kv_view.end())
			{
				do_flush_kv(user, kv_it->first, kv_it->second);
				// remove this key
				kv_view.erase(kv_it);
			}

			// else, do nothing
		};

		auto kv_flush_remaining =
				[&user, &kv_view]() -> void
		{
			for (const auto& kv: kv_view)
			{
				do_flush_kv(user, kv.first, kv.second);
				// note: newlines
				user << line_separator<std::basic_string_view<char_type>>;
			}
			// clear
			kv_view.clear();
		};

		return flush_to_user<ContextType>(
				file_path,
				group_user_handle<char_type>{
						.user = [&user]() -> decltype(auto)
						{ return user; },
						.contains =
								group_contains_type<char_type>{
										[&group_view](string_view_t<key_type> group_name) -> bool
										{
											return group_view.contains(group_name);
										}},
						.flush =
								group_flush_user_type<char_type>{
										[&user, &group_view, &kv_view, &kv_contains, &kv_flush, &kv_flush_remaining](const std::basic_string_view<char_type> group_name) -> kv_user_handle<char_type>
										{
											if (const auto group_it = group_view.find(group_name);
												group_it != group_view.end())
											{
												// flush head
												do_flush_group_head(user, group_name);

												// set current kvs view
												for (const auto& kv: *group_it->second)
												{
													kv_view.emplace(kv.first, kv.second);
												}

												// remove this group from view
												group_view.erase(group_name);

												return {
														.name			 = group_name,
														.contains		 = kv_contains,
														.flush			 = kv_flush,
														.flush_remaining = kv_flush_remaining};
											}

											return {};
										}},
						.flush_remaining =
								group_flush_remaining_user_type<char_type>{
										[&user, &group_view]() -> void
										{
											for (const auto& group: group_view)
											{
												// flush head
												do_flush_group_head(user, group.first);
												user << line_separator<key_type>;

												// kvs
												for (const auto& kv: *group.second)
												{
													do_flush_kv(user, kv.first, kv.second);
													user << line_separator<group_key_type>;
												}
											}

											// clear
											group_view.clear();
										}}});
	}
}// namespace gal::ini
