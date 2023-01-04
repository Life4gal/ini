#include <boost/ut.hpp>
#include <filesystem>
#include <fstream>
#include <ini/ini.hpp>

using namespace boost::ut;
using namespace gal::ini;

#define GROUP1_NAME "group1"
#define GROUP2_NAME "group2"
#define GROUP3_NAME "group3 !#@#*%$^&"
#define GROUP4_NAME "group4 }{}{}{}{}{}{()()()())[[[[[[["
#define GROUP5_NAME "group5 LKGP&ITIG&PG"

namespace
{
	suite generate_test_ini_reader_with_comment_file = []
	{
		const std::filesystem::path file_path{TEST_INI_READER_WITH_COMMENT_FILE_PATH};

		std::ofstream				file{file_path, std::ios::out | std::ios::trunc};

		file << "[" GROUP1_NAME << "]\n";
		file << "key1=value1\n";
		file << "key2 =value2\n";
		file << "key3 = value3\n";
		file << " key4  =       value4\n";
		file << "\n";

		file << "; comment1\n";
		file << "[" GROUP2_NAME "]# inline comment1\n";
		file << "# key1 comment\n";
		file << "key1       =           value1 ; key1 inline comment\n";
		file << "; key2 comment\n";
		file << "       key2=value2 # key2 inline comment\n";
		file << "key3=value3\n";
		file << "\n";

		file << "[" GROUP3_NAME "]\n";
		file << "   =       invalid line, ignore me\n";
		file << "key1=value1\n";
		file << " !@#$%^&*()_+ ignore me \n";
		file << "key2=value2\n";
		file << "ignore me\n";

		file << "[" GROUP4_NAME "]\n";

		file << "[" GROUP5_NAME "]\n";

		file.close();
	};

	suite test_ini_reader_with_comment_group_reader = []
	{
#if defined(GAL_INI_COMPILER_APPLE_CLANG)  || defined(GAL_INI_COMPILER_CLANG_CL)
		auto  workaround_extract_result_data = IniExtractorWithComment::extract_from_file(TEST_INI_READER_WITH_COMMENT_FILE_PATH);
		auto& extract_result				 = workaround_extract_result_data.first;
		auto& data							 = workaround_extract_result_data.second;
#else
		auto [extract_result, data] = IniExtractorWithComment::extract_from_file(TEST_INI_READER_WITH_COMMENT_FILE_PATH);
#endif
		"extract_ok"_test = [extract_result]
		{
			expect((extract_result == FileExtractResult::SUCCESS) >> fatal);
		};

		IniManagerWithComment parser{data};

		"group_size"_test = [&]
		{ expect((parser.size() == 5_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(parser.contains(GROUP1_NAME) >> fatal);
			expect(parser.contains(GROUP2_NAME) >> fatal);
			expect(parser.contains(GROUP3_NAME) >> fatal);
			expect(parser.contains(GROUP4_NAME) >> fatal);
			expect(parser.contains(GROUP5_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto reader = parser.read(string_view_type{GROUP1_NAME});

			expect((reader.name() == GROUP1_NAME) >> fatal);
			expect((reader.size() == 4_i) >> fatal);

			expect((reader.has_comment() != "group has comment"_b) >> fatal);
			expect((reader.has_inline_comment() != "group has inline comment"_b) >> fatal);

			expect(reader.contains("key1") >> fatal);
			expect(reader.contains("key2") >> fatal);
			expect(reader.contains("key3") >> fatal);
			expect(reader.contains("key4") >> fatal);

			expect((reader.has_comment("key1") != "key1 has comment"_b) >> fatal);
			expect((reader.has_comment("key2") != "key2 has comment"_b) >> fatal);
			expect((reader.has_comment("key3") != "key3 has comment"_b) >> fatal);
			expect((reader.has_comment("key4") != "key4 has comment"_b) >> fatal);

			expect((reader.has_inline_comment("key1") != "key1 has inline comment"_b) >> fatal);
			expect((reader.has_inline_comment("key2") != "key1 has inline comment"_b) >> fatal);
			expect((reader.has_inline_comment("key3") != "key1 has inline comment"_b) >> fatal);
			expect((reader.has_inline_comment("key4") != "key1 has inline comment"_b) >> fatal);

			expect((reader.get("key1") == "value1") >> fatal);
			expect((reader.get("key2") == "value2") >> fatal);
			expect((reader.get("key3") == "value3") >> fatal);
			expect((reader.get("key4") == "value4") >> fatal);
		};

		"group2"_test = [&]
		{
			const auto reader = parser.read(string_view_type{GROUP2_NAME});

			expect((reader.name() == GROUP2_NAME) >> fatal);
			expect((reader.size() == 3_i) >> fatal);

			expect((reader.has_comment() == "group has comment"_b) >> fatal);
			expect((reader.has_inline_comment() == "group has inline comment"_b) >> fatal);
			expect((reader.comment() == make_comment_view(CommentIndication::SEMICOLON, "comment1")) >> fatal);
			expect((reader.inline_comment() == make_comment_view(CommentIndication::HASH_SIGN, "inline comment1")) >> fatal);

			expect(reader.contains("key1") >> fatal);
			expect(reader.contains("key2") >> fatal);
			expect(reader.contains("key3") >> fatal);

			expect((reader.has_comment("key1") == "key1 has comment"_b) >> fatal);
			expect((reader.has_comment("key2") == "key2 has comment"_b) >> fatal);
			expect((reader.has_comment("key3") != "key has comment"_b) >> fatal);

			expect((reader.has_inline_comment("key1") == "key1 has comment"_b) >> fatal);
			expect((reader.has_inline_comment("key2") == "key2 has comment"_b) >> fatal);
			expect((reader.has_inline_comment("key3") != "key3 has comment"_b) >> fatal);

			expect((reader.comment("key1") == make_comment_view(CommentIndication::HASH_SIGN, "key1 comment")) >> fatal);
			expect((reader.comment("key2") == make_comment_view(CommentIndication::SEMICOLON, "key2 comment")) >> fatal);

			expect((reader.inline_comment("key1") == make_comment_view(CommentIndication::SEMICOLON, "key1 inline comment")) >> fatal);
			expect((reader.inline_comment("key2") == make_comment_view(CommentIndication::HASH_SIGN, "key2 inline comment")) >> fatal);

			expect((reader.get("key1") == "value1") >> fatal);
			expect((reader.get("key2") == "value2") >> fatal);
		};

		"group3"_test = [&]
		{
			const auto reader = parser.read(string_view_type{GROUP3_NAME});

			expect((reader.name() == GROUP3_NAME) >> fatal);
			expect((reader.size() == 2_i) >> fatal);

			expect(reader.contains("key1") >> fatal);
			expect(reader.contains("key2") >> fatal);

			expect((reader.get("key1") == "value1") >> fatal);
			expect((reader.get("key2") == "value2") >> fatal);
		};

		"group4"_test = [&]
		{
			const auto reader = parser.read(string_view_type{GROUP4_NAME});

			expect((reader.name() == GROUP4_NAME) >> fatal);
			expect(reader.empty() >> fatal);
			expect((reader.size() == 0_i) >> fatal);
		};

		"group5"_test = [&]
		{
			const auto reader = parser.read(string_view_type{GROUP5_NAME});

			expect((reader.name() == GROUP5_NAME) >> fatal);
			expect(reader.empty() >> fatal);
			expect((reader.size() == 0_i) >> fatal);
		};
	};

	suite test_ini_reader_with_comment_group_modifier = []
	{
#if defined(GAL_INI_COMPILER_APPLE_CLANG)  || defined(GAL_INI_COMPILER_CLANG_CL)
		auto  workaround_extract_result_data = IniExtractorWithComment::extract_from_file(TEST_INI_READER_WITH_COMMENT_FILE_PATH);
		auto& extract_result				 = workaround_extract_result_data.first;
		auto& data							 = workaround_extract_result_data.second;
#else
		auto [extract_result, data] = IniExtractorWithComment::extract_from_file(TEST_INI_READER_WITH_COMMENT_FILE_PATH);
#endif
		"extract_ok"_test = [extract_result]
		{
			expect((extract_result == FileExtractResult::SUCCESS) >> fatal);
		};

		IniManagerWithComment parser{data};

		"group_size"_test = [&]
		{ expect((parser.size() == 5_i) >> fatal); };

		"group_name"_test = [&]
		{
			expect(parser.contains(GROUP1_NAME) >> fatal);
			expect(parser.contains(GROUP2_NAME) >> fatal);
			expect(parser.contains(GROUP3_NAME) >> fatal);
			expect(parser.contains(GROUP4_NAME) >> fatal);
			expect(parser.contains(GROUP5_NAME) >> fatal);
		};

		"group1"_test = [&]
		{
			const auto writer = parser.write(string_view_type{GROUP1_NAME});

			expect((writer.name() == GROUP1_NAME) >> fatal);
			expect((writer.size() == 4_i) >> fatal);

			expect((writer.has_comment() != "group has comment"_b) >> fatal);
			expect((writer.has_inline_comment() != "group has inline comment"_b) >> fatal);

			expect(writer.contains("key1") >> fatal);
			expect(writer.contains("key2") >> fatal);
			expect(writer.contains("key3") >> fatal);
			expect(writer.contains("key4") >> fatal);

			expect((writer.has_comment("key1") != "key1 has comment"_b) >> fatal);
			expect((writer.has_comment("key2") != "key2 has comment"_b) >> fatal);
			expect((writer.has_comment("key3") != "key3 has comment"_b) >> fatal);
			expect((writer.has_comment("key4") != "key4 has comment"_b) >> fatal);

			expect((writer.has_inline_comment("key1") != "key1 has inline comment"_b) >> fatal);
			expect((writer.has_inline_comment("key2") != "key1 has inline comment"_b) >> fatal);
			expect((writer.has_inline_comment("key3") != "key1 has inline comment"_b) >> fatal);
			expect((writer.has_inline_comment("key4") != "key1 has inline comment"_b) >> fatal);

			expect((writer.get("key1") == "value1") >> fatal);
			expect((writer.get("key2") == "value2") >> fatal);
			expect((writer.get("key3") == "value3") >> fatal);
			expect((writer.get("key4") == "value4") >> fatal);
		};

		"group2"_test = [&]
		{
			const auto writer = parser.write(string_view_type{GROUP2_NAME});

			expect((writer.name() == GROUP2_NAME) >> fatal);
			expect((writer.size() == 3_i) >> fatal);

			expect((writer.has_comment() == "group has comment"_b) >> fatal);
			expect((writer.has_inline_comment() == "group has inline comment"_b) >> fatal);
			expect((writer.comment() == make_comment_view(CommentIndication::SEMICOLON, "comment1")) >> fatal);
			expect((writer.inline_comment() == make_comment_view(CommentIndication::HASH_SIGN, "inline comment1")) >> fatal);

			expect(writer.contains("key1") >> fatal);
			expect(writer.contains("key2") >> fatal);
			expect(writer.contains("key3") >> fatal);

			expect((writer.has_comment("key1") == "key1 has comment"_b) >> fatal);
			expect((writer.has_comment("key2") == "key2 has comment"_b) >> fatal);
			expect((writer.has_comment("key3") != "key has comment"_b) >> fatal);

			expect((writer.has_inline_comment("key1") == "key1 has comment"_b) >> fatal);
			expect((writer.has_inline_comment("key2") == "key2 has comment"_b) >> fatal);
			expect((writer.has_inline_comment("key3") != "key3 has comment"_b) >> fatal);

			expect((writer.comment("key1") == make_comment_view(CommentIndication::HASH_SIGN, "key1 comment")) >> fatal);
			expect((writer.comment("key2") == make_comment_view(CommentIndication::SEMICOLON, "key2 comment")) >> fatal);

			expect((writer.inline_comment("key1") == make_comment_view(CommentIndication::SEMICOLON, "key1 inline comment")) >> fatal);
			expect((writer.inline_comment("key2") == make_comment_view(CommentIndication::HASH_SIGN, "key2 inline comment")) >> fatal);

			expect((writer.get("key1") == "value1") >> fatal);
			expect((writer.get("key2") == "value2") >> fatal);
		};

		"group3"_test = [&]
		{
			const auto writer = parser.write(string_view_type{GROUP3_NAME});

			expect((writer.name() == GROUP3_NAME) >> fatal);
			expect((writer.size() == 2_i) >> fatal);

			expect(writer.contains("key1") >> fatal);
			expect(writer.contains("key2") >> fatal);

			expect((writer.get("key1") == "value1") >> fatal);
			expect((writer.get("key2") == "value2") >> fatal);
		};

		"group4"_test = [&]
		{
			const auto writer = parser.write(string_view_type{GROUP4_NAME});

			expect((writer.name() == GROUP4_NAME) >> fatal);
			expect(writer.empty() >> fatal);
			expect((writer.size() == 0_i) >> fatal);
		};

		"group5"_test = [&]
		{
			const auto writer = parser.write(string_view_type{GROUP5_NAME});

			expect((writer.name() == GROUP5_NAME) >> fatal);
			expect(writer.empty() >> fatal);
			expect((writer.size() == 0_i) >> fatal);
		};

		"add_group6"_test = [&]
		{
			auto writer = parser.write(string_type{"group6"});

			expect((parser.size() == 6_i) >> fatal);

			expect((writer.name() == "group6") >> fatal);
			expect((writer.empty() == "empty new group"_b) >> fatal);

			expect((writer.has_comment() != "group has comment"_b) >> fatal);
			expect((writer.has_inline_comment() != "group has inline comment"_b) >> fatal);

			writer.comment(make_comment(CommentIndication::HASH_SIGN, "group6 comment"));
			writer.inline_comment(make_comment(CommentIndication::SEMICOLON, "group6 inline comment"));

			expect((writer.has_comment() == "group has comment"_b) >> fatal);
			expect((writer.has_inline_comment() == "group has inline comment"_b) >> fatal);

			expect((writer.comment() == make_comment_view(CommentIndication::HASH_SIGN, "group6 comment")) >> fatal);
			expect((writer.inline_comment() == make_comment_view(CommentIndication::SEMICOLON, "group6 inline comment")) >> fatal);

			"add_key1"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.try_insert(
						"key1",
						"value1",
						make_comment(CommentIndication::HASH_SIGN, "key1 comment"),
						make_comment(CommentIndication::SEMICOLON, "key1 inline comment"));

				expect((result == "inserted"_b) >> fatal);

				expect((comment.empty() != "comment not empty"_b) >> fatal);
				expect((comment == make_comment_view(CommentIndication::HASH_SIGN, "key1 comment")) >> fatal);

				expect((key == "key1") >> fatal);
				expect((value == "value1") >> fatal);

				expect((inline_comment.empty() != "inline comment not empty"_b) >> fatal);
				expect((inline_comment == make_comment_view(CommentIndication::SEMICOLON, "key1 inline comment")) >> fatal);
			};

			"check key1"_test = [&]
			{
				expect((writer.size() == 1_i) >> fatal);
				expect((writer.contains("key1") == "key1 exists"_b) >> fatal);
				expect((writer.get("key1") == "value1") >> fatal);
				expect((writer.has_comment("key1") == "key1 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key1") == "key1 has inline comment"_b) >> fatal);
			};

			"add_key2"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.try_insert(
						"key2",
						"value2",
						make_comment(CommentIndication::HASH_SIGN, "key2 comment"));

				expect((result == "inserted"_b) >> fatal);

				expect((comment.empty() != "comment not empty"_b) >> fatal);
				expect((comment == make_comment_view(CommentIndication::HASH_SIGN, "key2 comment")) >> fatal);

				expect((key == "key2") >> fatal);
				expect((value == "value2") >> fatal);

				expect((inline_comment.empty() == "inline comment empty"_b) >> fatal);
			};

			"check_key2"_test = [&]
			{
				expect((writer.size() == 2_i) >> fatal);
				expect((writer.contains("key2") == "key2 exists"_b) >> fatal);
				expect((writer.get("key2") == "value2") >> fatal);
				expect((writer.has_comment("key2") == "key2 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key2") != "key2 has inline comment"_b) >> fatal);
			};

			"add_key3"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.try_insert(
						"key3",
						"value3");

				expect((result == "inserted"_b) >> fatal);

				expect((comment.empty() == "comment not empty"_b) >> fatal);

				expect((key == "key3") >> fatal);
				expect((value == "value3") >> fatal);

				expect((inline_comment.empty() == "inline comment empty"_b) >> fatal);
			};

			"check_key3"_test = [&]
			{
				expect((writer.size() == 3_i) >> fatal);
				expect((writer.contains("key3") == "key3 exists"_b) >> fatal);
				expect((writer.get("key3") == "value3") >> fatal);
				expect((writer.has_comment("key3") != "key3 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key3") != "key3 has inline comment"_b) >> fatal);
			};

			"add_key4"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.try_insert(
						"key4",
						"value4");

				expect((result == "inserted"_b) >> fatal);

				expect((comment.empty() == "comment not empty"_b) >> fatal);

				expect((key == "key4") >> fatal);
				expect((value == "value4") >> fatal);

				expect((inline_comment.empty() == "inline comment empty"_b) >> fatal);
			};

			"check_key4"_test = [&]
			{
				expect((writer.size() == 4_i) >> fatal);
				expect((writer.contains("key4") == "key4 exists"_b) >> fatal);
				expect((writer.get("key4") == "value4") >> fatal);
				expect((writer.has_comment("key4") != "key4 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key4") != "key4 has inline comment"_b) >> fatal);
			};

			"add_key5"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.try_insert(
						"key5",
						"value5");

				expect((result == "inserted"_b) >> fatal);

				expect((comment.empty() == "comment not empty"_b) >> fatal);

				expect((key == "key5") >> fatal);
				expect((value == "value5") >> fatal);

				expect((inline_comment.empty() == "inline comment empty"_b) >> fatal);
			};

			"check_key5"_test = [&]
			{
				expect((writer.size() == 5_i) >> fatal);
				expect((writer.contains("key5") == "key5 exists"_b) >> fatal);
				expect((writer.get("key5") == "value5") >> fatal);
				expect((writer.has_comment("key5") != "key5 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key5") != "key5 has inline comment"_b) >> fatal);
			};

			"assign_key4"_test = [&]
			{
				const auto& [result, comment, key, value, inline_comment] = writer.insert_or_assign(
						"key4",
						"value4",
						make_comment(CommentIndication::HASH_SIGN, "key4 comment"),
						make_comment(CommentIndication::SEMICOLON, "key4 inline comment"));

				expect((result != "inserted"_b) >> fatal);

				expect((comment.empty() != "comment not empty"_b) >> fatal);
				expect((comment == make_comment_view(CommentIndication::HASH_SIGN, "key4 comment")) >> fatal);

				expect((key == "key4") >> fatal);
				expect((value == "value4") >> fatal);

				expect((inline_comment.empty() != "inline comment not empty"_b) >> fatal);
				expect((inline_comment == make_comment_view(CommentIndication::SEMICOLON, "key4 inline comment")) >> fatal);
			};

			"check_assign_key4"_test = [&]
			{
				expect((writer.size() == 5_i) >> fatal);
				expect((writer.contains("key4") == "key4 exists"_b) >> fatal);
				expect((writer.get("key4") == "value4") >> fatal);
				expect((writer.has_comment("key4") == "key4 has comment"_b) >> fatal);
				expect((writer.has_inline_comment("key4") == "key4 has inline comment"_b) >> fatal);
			};

			"remove_key3"_test = [&]
			{ expect((writer.remove("key3") == "removed"_b) >> fatal); };

			"check_remove_key3"_test = [&]
			{
				expect((writer.size() == 4_i) >> fatal);
				expect((writer.contains("key3") != "key3 exists"_b) >> fatal);
			};

			"extract_key5_and_insert_back"_test = [&]
			{
				auto&& node					 = writer.extract("key5");

				"check_key5_not_exists"_test = [&]
				{
					expect((writer.size() == 3_i) >> fatal);
					expect((writer.contains("key5") != "key5 exists"_b) >> fatal);
				};

				auto& [comment, key, value, inline_comment] = node;
				comment										= make_comment(CommentIndication::HASH_SIGN, "key5 comment");
				value										= "new value5";

				"insert_key5_back"_test						= [&]
				{
					const auto& [result, result_comment, result_key, result_value, result_inline_comment] = writer.try_insert(std::move(node));

					expect((result == "inserted"_b) >> fatal);
					expect((result_comment == make_comment_view(CommentIndication::HASH_SIGN, "key5 comment")) >> fatal);
					expect((result_value == "new value5") >> fatal);
					expect((result_inline_comment.empty() == "inline comment empty"_b) >> fatal);
				};

				"check_key5_exists"_test = [&]
				{
					expect((writer.size() == 4_i) >> fatal);
					expect((writer.contains("key5") == "key5 exists"_b) >> fatal);
					expect((writer.get("key5") == "new value5") >> fatal);
					expect((writer.has_comment("key5") == "key5 has comment"_b) >> fatal);
					expect((writer.has_inline_comment("key5") != "key5 has inline comment"_b) >> fatal);
				};
			};

			"extract_key1_and_assign"_test = [&]
			{
				auto&& node					 = writer.extract("key1");

				"check_key1_not_exists"_test = [&]
				{
					expect((writer.size() == 3_i) >> fatal);
					expect((writer.contains("key1") != "key1 exists"_b) >> fatal);
				};

				"add_new_key1"_test = [&]
				{
					const auto& [inserted, comment, key, value, inline_comment] = writer.try_insert(
							"key1",
							"new value1",
							{},
							make_comment(CommentIndication::SEMICOLON, "new inline comment"));

					expect((inserted == "inserted"_b) >> fatal);

					expect((comment.empty() == "empty comment"_b) >> fatal);

					expect((key == "key1") >> fatal);
					expect((value == "new value1") >> fatal);

					expect((inline_comment.empty() != "empty inline comment"_b) >> fatal);
					expect((inline_comment == make_comment_view(CommentIndication::SEMICOLON, "new inline comment")) >> fatal);
				};

				auto& [comment, key, value, inline_comment] = node;
				comment										= make_comment(CommentIndication::HASH_SIGN, "key5 new comment");
				inline_comment								= {};

				"insert_key1_back"_test						= [&]
				{
					const auto& [inserted, inserted_comment, inserted_key, inserted_value, inserted_inline_comment] = writer.insert_or_assign(std::move(node));

					expect((inserted != "inserted"_b) >> fatal);

					expect((inserted_comment.empty() != "empty comment"_b) >> fatal);
					expect((inserted_comment == make_comment_view(CommentIndication::HASH_SIGN, "key5 new comment")) >> fatal);

					expect((inserted_key == "key1") >> fatal);
					expect((inserted_value == "value1") >> fatal);

					expect((inserted_inline_comment.empty() == "empty inline comment"_b) >> fatal);
				};

				"check_key1_exists"_test = [&]
				{
					expect((writer.size() == 4_i) >> fatal);
					expect((writer.contains("key1") == "key1 exists"_b) >> fatal);
					expect((writer.get("key1") == "value1") >> fatal);
					expect((writer.has_comment("key1") == "key1 has comment"_b) >> fatal);
					expect((writer.has_inline_comment("key1") != "key1 has inline comment"_b) >> fatal);
				};
			};
		};
	};
}// namespace
