#pragma once

#include <ini/impl/group_accessor.hpp>

namespace gal::ini::impl
{
	enum class FileExtractResult
	{
		// The file was not found.
		FILE_NOT_FOUND,
		// The file cannot be opened.
		PERMISSION_DENIED,
		// An internal OS error, such as failure to read from the file.
		INTERNAL_ERROR,

		SUCCESS,
	};

	/**
	 * @brief IniExtractor is only responsible for extracting data from the file, it does not store data itself and does not modify it. (This class should not even be instantiated.)
	 */
	class IniExtractor
	{
	public:
		using file_path_type = string_view_type;
		using buffer_type	 = string_view_type;

		using write_type	 = GroupAccessorReadModify;
		// [group_name]
		// key1 = value1
		// key2 = value2
		// key3 = value3
		using group_type	 = write_type::group_type;
		// [group1]
		// ...
		// [group2]
		// ...
		// [group...n]
		using context_type	 = unordered_table_type<group_type>;

		/**
		 * @brief Reads data from a file and writes the result to out.
		 * @param file_path The file path.
		 * @param out The dest to write.
		 * @return The result of reading the file.
		 */
		static auto extract_from_file(file_path_type file_path, context_type& out) -> FileExtractResult;

		/**
		 * @brief Reads data from a file and return result of reading the file.
		 * @param file_path The file path.
		 * @return The result of reading the file.
		 */
		static auto extract_from_file(file_path_type file_path) -> std::pair<FileExtractResult, context_type>;

		/**
		 * @brief Reads data from a memory buffer and writes the result to out.
		 * @param string_buffer The memory buffer.
		 * @param out The dest to write.
		 */
		static auto extract_from_buffer(buffer_type string_buffer, context_type& out) -> void;

		/**
		 * @brief Reads data from a memory buffer and return result of reading the file.
		 * @param string_buffer The memory buffer.
		 * @return The result of reading the memory buffer.
		 */
		static auto extract_from_buffer(buffer_type string_buffer) -> context_type;
	};

	/**
	 * @brief IniExtractorWithComment is only responsible for extracting data from the file, it does not store data itself and does not modify it. (This class should not even be instantiated.)
	 */
	class IniExtractorWithComment
	{
	public:
		using file_path_type = string_view_type;
		using buffer_type	 = string_view_type;

		using write_type	 = GroupAccessorReadModifyWithComment;
		// # group_comment
		// [group_name] # inline_group_comment
		// ; comment1
		// key1 = value1 ;inline_comment1
		// key2 = value2
		// key3 = value3
		using group_type	 = write_type::group_type;
		// [group1]
		// ...
		// [group2]
		// ...
		// [group...n]
		using context_type	 = unordered_table_type<group_type>;

		/**
		 * @brief Reads data from a file and writes the result to out.
		 * @param file_path The file path.
		 * @param out The dest to write.
		 * @return The result of reading the file.
		 */
		static auto extract_from_file(file_path_type file_path, context_type& out) -> FileExtractResult;

		/**
		 * @brief Reads data from a file and return result of reading the file.
		 * @param file_path The file path.
		 * @return The result of reading the file.
		 */
		static auto extract_from_file(file_path_type file_path) -> std::pair<FileExtractResult, context_type>;

		/**
		 * @brief Reads data from a memory buffer and writes the result to out.
		 * @param string_buffer The memory buffer.
		 * @param out The dest to write.
		 */
		static auto extract_from_buffer(buffer_type string_buffer, context_type& out) -> void;

		/**
		 * @brief Reads data from a memory buffer and return result of reading the file.
		 * @param string_buffer The memory buffer.
		 * @return The result of reading the memory buffer.
		 */
		static auto extract_from_buffer(buffer_type string_buffer) -> context_type;
	};
}// namespace gal::ini::impl
