#include <algorithm>
#include <ini/ini.hpp>
#include <ranges>

namespace gal::ini::impl
{
	GroupAccessorWriteOnly::GroupAccessorWriteOnly(const GroupAccessorReadOnly::group_type& group)
	{
#if defined(GAL_INI_COMPILER_APPLE_CLANG)
		for (const auto& [key, value]: group)
		{
			group_.emplace(key, value);
		}
#else
		std::ranges::transform(
				group,
				std::inserter(group_, group_.end()),
				[](const auto& pair) -> group_type::value_type
				{ return group_type::value_type{pair.first, pair.second}; });
#endif
	}

	GroupAccessorWriteOnlyWithComment::GroupAccessorWriteOnlyWithComment(const GroupAccessorReadOnlyWithComment::group_type& group)
	{
#if defined(GAL_INI_COMPILER_APPLE_CLANG)
		for (const auto& [key, value_with_comment]: group.variables)
		{
			group_.variables.emplace(key, variable_with_comment{.comment = value_with_comment.comment, .variable = value_with_comment.variable, .inline_comment = value_with_comment.inline_comment});
		}
#else
		std::ranges::transform(
				group.variables,
				std::inserter(group_.variables, group_.variables.end()),
				[](const auto& pair) -> variables_type::value_type
				{ return variables_type::value_type{pair.first, variable_with_comment{.comment = pair.second.comment, .variable = pair.second.variable, .inline_comment = pair.second.inline_comment}}; });
#endif

		group_.comment		  = group.comment;
		group_.inline_comment = group.inline_comment;
	}
}// namespace gal::ini::impl
