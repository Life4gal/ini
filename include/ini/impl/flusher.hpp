#pragma once

#include <ini/impl/extractor.hpp>
#include <ini/impl/group_accessor.hpp>

namespace gal::ini::impl
{
	enum class FileFlushResult
	{
		// The file was not found.
		FILE_NOT_FOUND,
		// The file cannot be opened.
		PERMISSION_DENIED,
		// An internal OS error, such as failure to read from the file.
		INTERNAL_ERROR,

		SUCCESS,
	};

	class IniFlusher
	{
	public:
		using group_type	 = IniExtractor::group_type;
		using context_type	 = IniExtractor::context_type;

		using write_type	 = GroupAccessorWriteOnly;

		using file_path_type = IniExtractor::file_path_type;

	private:
		const context_type& context_;

	public:
		explicit IniFlusher(const context_type& context)
			: context_{context} {}

		/**
		 * @brief Get the flusher of the target group.
		 * @param group_name The group's name.
		 * @return The flush type.
		 */
		auto flush(const string_view_type group_name) -> write_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return write_type{it->second}; }

			return write_type{{}};
		}

		/**
		 * @brief Flush the currently saved content into the out.
		 * @param out The destination.
		 */
		auto flush(ostream_type& out) -> void
		{
			for (const auto& group: context_)
			{
				write_type writer{group.second};
				writer.flush_remainder(out);
				out << line_separator;
			}
		}

		/**
		 * @brief Flush the currently saved content into the file, and keep the original order.
		 * @param file_path The file path.
		 * @param keep_comments Whether to keep the comments. (We read the content ignoring the comments, and can decide whether to remove the original comments when writing back to the file)
		 * @param keep_empty_group Whether to keep empty groups. (If a group does not have any values, we can decide if we want to keep the group in the file or not)
		 * @return Whether the flush was successful or not.
		 * @note If the target file does not exist, nothing is done (the flush is considered to have failed), and no order is guaranteed for the new (non-existent in the source file) data.
		 */
		auto flush_override(file_path_type file_path, bool keep_comments, bool keep_empty_group) const -> FileFlushResult;
	};

	class IniFlusherWithComment
	{
	public:
		using group_type	 = IniExtractorWithComment::group_type;
		using context_type	 = IniExtractorWithComment::context_type;

		using write_type	 = GroupAccessorWriteOnlyWithComment;

		using file_path_type = IniExtractorWithComment::file_path_type;

	private:
		const context_type& context_;

	public:
		explicit IniFlusherWithComment(const context_type& context)
			: context_{context} {}

		/**
		 * @brief Get the flusher of the target group.
		 * @param group_name The group's name.
		 * @return The flush type.
		 */
		auto flush(const string_view_type group_name) -> write_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return write_type{it->second}; }

			return write_type{{}};
		}

		/**
		 * @brief Flush the currently saved content into the out.
		 * @param out The destination.
		 */
		auto flush(ostream_type& out) -> void
		{
			for (const auto& group: context_)
			{
				write_type writer{group.second};
				writer.flush_remainder(out);
				out << line_separator;
			}
		}

		/**
		 * @brief Flush the currently saved content into the file, and keep the original order.
		 * @param file_path The file path.
		 * @param keep_comment Whether to keep the comments. (We read the content ignoring the comments, and can decide whether to remove the original comments when writing back to the file)
		 * @param keep_empty_group Whether to keep empty groups. (If a group does not have any values, we can decide if we want to keep the group in the file or not)
		 * @return Whether the flush was successful or not.
		 * @note If the target file does not exist, nothing is done (the flush is considered to have failed), and no order is guaranteed for the new (non-existent in the source file) data.
		 */
		auto flush_override(file_path_type file_path, bool keep_empty_group) const -> FileFlushResult;
	};
}// namespace gal::ini::impl
