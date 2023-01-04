#pragma once

#include <ini/impl/group_accessor.hpp>

namespace gal::ini::impl
{
	class IniManager
	{
	public:
		using group_type   = IniExtractor::group_type;
		using context_type = IniExtractor::context_type;

		using reader_type  = GroupAccessorReadOnly;
		using writer_type  = GroupAccessorReadModify;

	private:
		context_type& context_;

	public:
		explicit IniManager(context_type& context)
			: context_{context} {}

		/**
		 * @brief Get whether the file is empty.
		 * @return The file is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return context_.empty(); }

		/**
		 * @brief Get the number of groups in the file.
		 * @return The number of groups in the file.
		 */
		[[nodiscard]] auto size() const noexcept -> context_type::size_type { return context_.size(); }

		/**
		 * @brief Check whether the file contains the group.
		 * @param group_name The group to find.
		 * @return The file contains the group or not.
		 */
		[[nodiscard]] auto contains(const string_view_type group_name) const -> bool { return context_.contains(group_name); }

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return reader_type{it->first, it->second}; }

			// todo
			const static group_type empty_group{};
			return reader_type{{}, empty_group};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 * @note Reading a group that does not exist is usually meaningless, because the reader cannot modify any data.
		 */
		[[nodiscard]] auto read(const string_view_type group_name) -> reader_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return reader_type{it->first, it->second}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return reader_type{result->first, result->second};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto write(const string_type& group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->first, it->second}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return writer_type{result->first, result->second};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->first, it->second}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return writer_type{result->first, result->second};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto write(string_type&& group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->first, it->second}; }

			const auto result = context_.emplace(std::move(group_name), group_type{}).first;
			return writer_type{result->first, result->second};
		}
	};

	class IniManagerWithComment
	{
	public:
		using group_type   = IniExtractorWithComment::group_type;
		using context_type = IniExtractorWithComment::context_type;

		using reader_type  = GroupAccessorReadOnlyWithComment;
		using writer_type  = GroupAccessorReadModifyWithComment;

	private:
		context_type& context_;

	public:
		explicit IniManagerWithComment(context_type& context)
			: context_{context} {}

		/**
		 * @brief Get whether the file is empty.
		 * @return The file is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return context_.empty(); }

		/**
		 * @brief Get the number of groups in the file.
		 * @return The number of groups in the file.
		 */
		[[nodiscard]] auto size() const noexcept -> context_type::size_type { return context_.size(); }

		/**
		 * @brief Check whether the file contains the group.
		 * @param group_name The group to find.
		 * @return The file contains the group or not.
		 */
		[[nodiscard]] auto contains(const string_view_type group_name) const -> bool { return context_.contains(group_name); }

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return reader_type{it->first, it->second}; }

			// todo
			const static group_type empty_group{};
			return reader_type{{}, empty_group};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 * @note Reading a group that does not exist is usually meaningless, because the reader cannot modify any data.
		 */
		[[nodiscard]] auto read(const string_view_type group_name) -> reader_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return reader_type{it->first, it->second}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return reader_type{result->first, result->second};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
		{
			if (const auto it = table_finder{}(context_, group_name);
				it != context_.end()) { return writer_type{it->first, it->second}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return writer_type{result->first, result->second};
		}

		/**
		 * @brief Get the data of a group. (If the group does not exist, a group with an empty name and empty content is created and returned)
		 * @param group_name The group's name.
		 * @return The reader of the group.
		 */
		[[nodiscard]] auto write(string_type&& group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->first, it->second}; }

			const auto result = context_.emplace(std::move(group_name), group_type{}).first;
			return writer_type{result->first, result->second};
		}
	};
}// namespace gal::ini::impl
