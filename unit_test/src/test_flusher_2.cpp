#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/extractor.hpp>
#include <ini/flusher.hpp>
#include <map>
#include <string>

using namespace boost::ut;
using namespace gal::ini;

#define GROUP1_NAME "group1"
#define GROUP2_NAME "group2"
#define GROUP3_NAME "group3 !#@#*%$^&"
#define GROUP4_NAME "group4 }{}{}{}{}{}{()()()())[[[[[[["
#define GROUP5_NAME "group5 LKGP&ITIG&PG"

#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
#define GAL_INI_NO_DESTROY [[clang::no_destroy]]
#define GAL_INI_CLANG_WORKAROUND_DEDUCTION <char_type>
#else
#define GAL_INI_NO_DESTROY
#define GAL_INI_CLANG_WORKAROUND_DEDUCTION
#endif

#if !defined(GAL_INI_NO_DEBUG_FLUSH)
#if !defined(NODEBUG) && !defined(_NODEBUG)
#define GAL_INI_DEBUG_FLUSH_REQUIRED
#endif
#endif

#if defined(GAL_INI_DEBUG_FLUSH_REQUIRED)
#define GAL_INI_DEBUG_FLUSH_NEW_LINE std::endl
#else
	#define GAL_INI_DEBUG_FLUSH_NEW_LINE line_separator<string_view_t<char_type>>
#endif

namespace
{
	using group_type = std::map<std::string, std::string, std::less<>>;
	using context_type = std::map<std::string, group_type, std::less<>>;

	#if not defined(GAL_INI_COMPILER_MSVC)
	GAL_INI_NO_DESTROY context_type data{};

	#define GAL_INI_MSVC_WORKAROUND_DATA
	#else
	[[nodiscard]] auto msvc_workaround_get_data() -> context_type&
	{
		static context_type data{};
		return data;
	}

	// todo: It looks like MSVC doesn't ensure that the `data` construction is done before the execution of the `suite`?
	#define GAL_INI_MSVC_WORKAROUND_DATA auto& data = msvc_workaround_get_data();
	#endif

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_flusher_generate_data = []
	{
		GAL_INI_MSVC_WORKAROUND_DATA

		{
			auto& group = data[GROUP1_NAME];

			group.emplace("key1", "value1");
			group.emplace("key2", "value2");
			group.emplace("key3", "value3");
			group.emplace("key4", "");
		}

		{
			auto& group = data[GROUP2_NAME];

			group.emplace("key1", "value1");
			group.emplace("key2", "value2");
		}

		{
			data.emplace(GROUP3_NAME, group_type{});
			data.emplace(GROUP4_NAME, group_type{});
			data.emplace(GROUP5_NAME, group_type{});
		}
	};

	auto check_initial_data = [](const ExtractResult extract_result, const context_type& this_data) -> void
	{
		"extract_ok"_test = [extract_result] { expect((extract_result == ExtractResult::SUCCESS) >> fatal); };

		"group_size"_test = [&] { expect((this_data.size() == 5_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(this_data.contains(GROUP1_NAME) >> fatal);
			expect(this_data.contains(GROUP2_NAME) >> fatal);
			expect(this_data.contains(GROUP3_NAME) >> fatal);
			expect(this_data.contains(GROUP4_NAME) >> fatal);
			expect(this_data.contains(GROUP5_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto [name, group] = *this_data.find(GROUP1_NAME);

			expect((name == GROUP1_NAME) >> fatal);
			expect((group.size() == 4_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);
			expect(group.contains("key3") >> fatal);
			expect(group.contains("key4") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
			expect((group.at("key3") == "value3") >> fatal);
			expect((group.at("key4").empty()) >> fatal);
		};

		"group2"_test = [&]
		{
			const auto [name, group] = *this_data.find(GROUP2_NAME);

			expect((name == GROUP2_NAME) >> fatal);
			expect((group.size() == 2_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
		};

		"group3"_test = [&]
		{
			const auto [name, group] = *this_data.find(GROUP3_NAME);

			expect((name == GROUP3_NAME) >> fatal);
			expect((group.empty()) >> fatal);
		};

		"group4"_test = [&]
		{
			const auto [name, group] = *this_data.find(GROUP4_NAME);

			expect((name == GROUP4_NAME) >> fatal);
			expect((group.empty()) >> fatal);
		};

		"group5"_test = [&]
		{
			const auto [name, group] = *this_data.find(GROUP5_NAME);

			expect((name == GROUP5_NAME) >> fatal);
			expect((group.empty()) >> fatal);
		};
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_flusher_initial_data = []
	{
		GAL_INI_MSVC_WORKAROUND_DATA

		check_initial_data(ExtractResult::SUCCESS, data);
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_flusher_flush_to_file = []
	{
		GAL_INI_MSVC_WORKAROUND_DATA

		flush_to_file(TEST_INI_FLUSHER_FILE_PATH, data);

		#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
		auto  workaround_extract_result_data = extract_from_file<context_type>(TEST_INI_FLUSHER_FILE_PATH);
		auto& extract_result                 = workaround_extract_result_data.first;
		auto& extract_data                   = workaround_extract_result_data.second;
		#else
		auto [extract_result, extract_data] = extract_from_file<context_type>(TEST_INI_FLUSHER_FILE_PATH);
		#endif

		check_initial_data(extract_result, extract_data);
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_flusher_flush_to_file_keep_empty_group = []
	{
		GAL_INI_MSVC_WORKAROUND_DATA

		{
			using key_type = context_type::key_type;

			using group_key_type = group_type::key_type;
			using group_mapped_type = group_type::mapped_type;

			using char_type = string_view_t<key_type>::value_type;

			using group_view_type = common::map_type_t<context_type, string_view_t<key_type>, const group_type*>;
			using kv_view_type = common::map_type_t<group_type, string_view_t<group_key_type>, string_view_t<group_mapped_type>>;

			constexpr static auto do_flush_group_head = [](std::basic_ostream<char_type>& out, const string_view_t<char_type> group_name) -> void
			{
				// '[' group_name ']' ; foo bar baz here
				// no '\n', see `group_flush_type`
				out
						<< square_bracket<key_type>.first
						<< group_name
						<< square_bracket<key_type>.second
						// write something random, for demonstration purposes only, this content is considered a comment.
						<< " ; foo bar baz here";
			};
			constexpr static auto do_flush_kv = [](std::basic_ostream<char_type>& out, const string_view_t<char_type> key, const string_view_t<char_type> value) -> void
			{
				// key 'space' '=' 'space' value ; foo bar baz here
				// no '\n', see `kv_flush_type`
				out
						<< key
						<< blank_separator<group_key_type> << kv_separator<group_key_type> << blank_separator<group_key_type> << value
						// write something random, for demonstration purposes only, this content is considered a comment.
						<< " # foo bar baz here";
			};

			// We need the following two temporary variables to hold some necessary information, and they must have a longer lifetime than the incoming StackFunction.

			// all group view
			auto group_view = [](const auto& gs) -> group_view_type
			{
				group_view_type vs{};
				for (const auto& g: gs) { vs.emplace(g.first, &g.second); }
				return vs;
			}(data);

			// current kvs view
			kv_view_type kv_view{};

			// see flusher.hpp::flush_to_file(std::string_view file_path, ContextType& in)
			auto                                                   kv_contains =
					[&kv_view](const string_view_t<group_key_type> key) -> bool { return kv_view.contains(key); };

			auto                                              kv_flush =
					[&kv_view](std::basic_ostream<char_type>& out, const string_view_t<char_type> key) -> void
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

			auto                                              kv_flush_remaining =
					[&kv_view](std::basic_ostream<char_type>& out) -> void
			{
				for (const auto& [key, value]: kv_view)
				{
					do_flush_kv(out, key, value);
					// note: newlines
					out << GAL_INI_DEBUG_FLUSH_NEW_LINE;
				}
				// clear
				kv_view.clear();
			};

			flush_to_file<context_type>(
					TEST_INI_FLUSHER_FILE_PATH,
					group_ostream_handle GAL_INI_CLANG_WORKAROUND_DEDUCTION{
							.contains =
							group_contains_type<char_type>{
									[&group_view](const string_view_t<key_type> group_name) -> bool
									{
										// return group_view.contains(group_name);
										// remove empty group
										if (const auto group_it = group_view.find(group_name);
											group_it != group_view.end()) { return !group_it->second->empty(); }
										return false;
									}},
							.flush =
							group_flush_ostream_type<char_type>{
									[&group_view, &kv_view, &kv_contains, &kv_flush, &kv_flush_remaining](std::basic_ostream<char_type>& out, const string_view_t<char_type> group_name) -> kv_ostream_handle<char_type>
									{
										if (const auto group_it = group_view.find(group_name);
											group_it != group_view.end())
										{
											// flush head
											do_flush_group_head(out, group_name);

											// set current kvs view
											for (const auto& kv: *group_it->second) { kv_view.emplace(kv.first, kv.second); }

											// remove this group from view
											group_view.erase(group_name);

											return {
													.name = group_name,
													.contains = kv_contains,
													.flush = kv_flush,
													.flush_remaining = kv_flush_remaining};
										}

										return {};
									}},
							.flush_remaining =
							group_flush_remaining_ostream_type<char_type>{
									[&group_view](std::basic_ostream<char_type>& out) -> void
									{
										for (const auto& [name, group]: group_view)
										{
											// remove empty group
											if (group->empty()) { continue; }

											// flush head
											do_flush_group_head(out, name);
											out << GAL_INI_DEBUG_FLUSH_NEW_LINE;

											// kvs
											for (const auto& kv: *group)
											{
												do_flush_kv(out, kv.first, kv.second);
												out << GAL_INI_DEBUG_FLUSH_NEW_LINE;
											}
										}

										// clear
										group_view.clear();
									}}});
		}

		#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
		auto  workaround_extract_result_data = extract_from_file<context_type>(TEST_INI_FLUSHER_FILE_PATH);
		auto& extract_result                 = workaround_extract_result_data.first;
		auto& extract_data                   = workaround_extract_result_data.second;
		#else
		auto [extract_result, extract_data] = extract_from_file<context_type>(TEST_INI_FLUSHER_FILE_PATH);
		#endif

		"extract_ok"_test = [extract_result] { expect((extract_result == ExtractResult::SUCCESS) >> fatal); };

		"group_size"_test = [&] { expect((extract_data.size() == 2_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(extract_data.contains(GROUP1_NAME) >> fatal);
			expect(extract_data.contains(GROUP2_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto [name, group] = *extract_data.find(GROUP1_NAME);

			expect((name == GROUP1_NAME) >> fatal);
			expect((group.size() == 4_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);
			expect(group.contains("key3") >> fatal);
			expect(group.contains("key4") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
			expect((group.at("key3") == "value3") >> fatal);
			expect((group.at("key4").empty()) >> fatal);
		};

		"group2"_test = [&]
		{
			const auto [name, group] = *extract_data.find(GROUP2_NAME);

			expect((name == GROUP2_NAME) >> fatal);
			expect((group.size() == 2_i) >> fatal);

			expect(group.contains("key1") >> fatal);
			expect(group.contains("key2") >> fatal);

			expect((group.at("key1") == "value1") >> fatal);
			expect((group.at("key2") == "value2") >> fatal);
		};
	};

	GAL_INI_NO_DESTROY [[maybe_unused]] suite test_ini_flusher_flush_to_user = []
	{
		GAL_INI_MSVC_WORKAROUND_DATA

		using key_type = context_type::key_type;
		using char_type = string_view_t<key_type>::value_type;

		key_type buffer{};

		class MyOut final : public UserOut<char_type>
		{
		public:
			using out_type = key_type;

		private:
			out_type& out_;

		public:
			explicit MyOut(out_type& out)
				: out_{out} {}


			/* constexpr */
			auto operator<<(const char_type d) -> UserOut&
				// Tell me why! MSVC!
				#if not defined(GAL_INI_COMPILER_MSVC)
				override
				#endif
			{
				out_.push_back(d);
				return *this;
			}

			/* constexpr */
			auto operator<<(const string_view_t<char_type> d) -> UserOut&
				// Tell me why! MSVC!
				#if not defined(GAL_INI_COMPILER_MSVC)
				override
				#endif
			{
				out_.append(d);
				return *this;
			}
		};

		MyOut out{buffer};

		flush_to_user(TEST_INI_FLUSHER_FILE_PATH, data, out);

		#if defined(GAL_INI_COMPILER_APPLE_CLANG) || defined(GAL_INI_COMPILER_CLANG_CL) || defined(GAL_INI_COMPILER_CLANG)
			auto  workaround_extract_result_data = extract_from_buffer<context_type>(buffer);
			auto& extract_result                 = workaround_extract_result_data.first;
			auto& extract_data                   = workaround_extract_result_data.second;
		#else
		auto [extract_result, extract_data] = extract_from_buffer<context_type>(buffer);
		#endif

		check_initial_data(extract_result, extract_data);
	};
}// namespace
