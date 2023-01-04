#pragma once

#include <type_traits>

namespace gal::ini::impl
{
	struct table_finder
	{
		// cxx 20
		template<typename Table>
		struct transparent : std::false_type
		{
		};
		template<typename Table>
			requires requires {
						 {
							 std::declval<Table&>().find(std::declval<const string_view_type>())
						 } -> std::same_as<typename Table::iterator>;

						 {
							 std::declval<const Table&>().find(std::declval<const string_view_type>())
						 } -> std::same_as<typename Table::const_iterator>;
					 }
		struct transparent<Table> : std::true_type
		{
		};
		template<typename Table>
		constexpr static bool transparent_v = transparent<Table>::value;

		template<typename Table>
		[[nodiscard]] constexpr auto operator()(Table& table, const string_type& key) -> typename Table::iterator
		{
			return table.find(key);
		}

		template<typename Table>
		[[nodiscard]] constexpr auto operator()(const Table& table, const string_type& key) -> typename Table::const_iterator
		{
			return table.find(key);
		}

		template<typename Table>
		[[nodiscard]] constexpr auto operator()(Table& table, const string_view_type key) -> typename Table::iterator
		{
			if constexpr (transparent_v<Table>)
			{
				return table.find(key);
			}
			else
			{
				return this->operator()(table, string_type{key});
			}
		}

		template<typename Table>
		[[nodiscard]] constexpr auto operator()(const Table& table, const string_view_type key) -> typename Table::const_iterator
		{
			if constexpr (transparent_v<Table>)
			{
				return table.find(key);
			}
			else
			{
				return this->operator()(table, string_type{key});
			}
		}
	};

	struct table_modifier
	{
		// cxx 23
		template<typename Table>
		struct transparent : std::false_type
		{
		};
		template<typename Table>
			requires requires {
						 std::declval<Table&>().erase(std::declval<const string_view_type>());

						 {
							 std::declval<Table&>().extract(std::declval<const string_view_type>())
						 } -> std::same_as<typename Table::node_type>;
					 }
		struct transparent<Table> : std::true_type
		{
		};
		template<typename Table>
		constexpr static bool transparent_v = transparent<Table>::value;

		template<typename Table>
		[[nodiscard]] constexpr static auto erase(Table& table, const string_type& key) -> decltype(auto)
		{
			return table.erase(key);
		}

		template<typename Table>
		[[nodiscard]] constexpr static auto erase(Table& table, const string_view_type key) -> decltype(auto)
		{
			if constexpr (transparent_v<Table>)
			{
				return table.erase(key);
			}
			else
			{
				return erase(table, string_type{key});
			}
		}

		template<typename Table>
		[[nodiscard]] constexpr static auto extract(Table& table, const string_type& key) -> typename Table::node_type
		{
			return table.extract(key);
		}

		template<typename Table>
		[[nodiscard]] constexpr static auto extract(Table& table, const string_view_type key) -> typename Table::node_type
		{
			if constexpr (transparent_v<Table>)
			{
				return table.extract(key);
			}
			else
			{
				return extract(table, string_type{key});
			}
		}
	};

	class GroupAccessorReadOnly;
	class GroupAccessorReadModify;
	class GroupAccessorReadOnlyWithComment;
	class GroupAccessorReadModifyWithComment;
	class GroupAccessorWriteOnly;
	class GroupAccessorWriteOnlyWithComment;

	class GroupAccessorReadOnly
	{
	public:
		using group_type = unordered_table_type<string_type>;

	private:
		string_view_type  name_;
		const group_type& group_;

	public:
		explicit GroupAccessorReadOnly(const string_view_type name, const group_type& group)
			: name_{name},
			  group_{group} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] auto name() const noexcept -> string_view_type { return name_; }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return group_.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
		{
			if (const auto it = table_finder{}(group_, key);
				it != group_.end())
			{
				return it->second;
			}

			return {};
		}
	};

	class GroupAccessorReadModify
	{
		using read_accessor = GroupAccessorReadOnly;

	public:
		using group_type = read_accessor::group_type;

		class Node
		{
			friend GroupAccessorReadModify;

		public:
			constexpr static std::size_t max_elements_size = 2;

			template<std::size_t Index>
			// requires (Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					string_type,
					std::conditional_t<
							Index == 1,
							string_type,
							void>>;

		private:
			group_type::node_type node_;

			explicit Node(group_type::node_type&& node)
				: node_{std::move(node)} {}

			explicit(false) operator group_type::node_type&&() && { return std::move(node_); }

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() const& -> const index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.key(); }
				else if constexpr (Index == 1) { return node_.mapped(); }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() & -> index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.key(); }
				else if constexpr (Index == 1) { return node_.mapped(); }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() && -> index_type<Index>&&
			{
				if constexpr (Index == 0) { return std::move(node_.key()); }
				else if constexpr (Index == 1) { return std::move(node_.mapped()); }
				else { GAL_INI_UNREACHABLE(); }
			}

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() const& -> string_view_type { return get<0>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() & -> string_view_type { return get<0>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() && -> string_type&& { return std::move(*this).get<0>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() const& -> string_view_type { return get<1>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() & -> string_view_type { return get<1>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() && -> string_type&& { return std::move(*this).get<1>(); }

			/**
			 * @brief Determine if the node is valid (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] explicit operator bool() const noexcept { return node_.operator bool(); }

			/**
			 * @brief Determine if the node is valid. (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] auto	   empty() const noexcept -> bool { return node_.empty(); }
		};

		using node_type = Node;

		class InsertResult
		{
			friend GroupAccessorReadModify;

		public:
			constexpr static std::size_t max_elements_size = node_type::max_elements_size + 1;

			template<std::size_t Index>
				requires(Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					bool,
					node_type::index_type<Index - 1>>;

		private:
			using result_type = std::pair<group_type::const_iterator, bool>;

			result_type result_;

			explicit InsertResult(result_type result)
				: result_{result} {}

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() const -> const index_type<Index>&
			{
				if constexpr (Index == 0) { return result_.second; }
				else if constexpr (Index == 1) { return result_.first->first; }
				else if constexpr (Index == 2) { return result_.first->second; }
				else { GAL_INI_UNREACHABLE(); }
			}

			/**
			 * @brief Determine if the insertion was successful.
			 * @return The insertion was successes or not.
			 * @note If it overwrites an existing value, it is considered not inserted.
			 */
			[[nodiscard]] explicit operator bool() const noexcept { return get<0>(); }

			/**
			 * @brief Determine if the insertion was successful.
			 * @return The insertion was successes or not.
			 * @note If it overwrites an existing value, it is considered not inserted.
			 */
			[[nodiscard]] auto	   result() const -> bool { return get<0>(); }

			/**
			 * @brief Get the inserted key.
			 * @return The key of the insertion.
			 */
			[[nodiscard]] auto	   key() const -> string_view_type { return get<1>(); }

			/**
			 * @brief Get the inserted value.
			 * @return The value of the insertion.
			 */
			[[nodiscard]] auto	   value() const -> string_view_type { return get<2>(); }
		};

		using result_type = InsertResult;

	private:
		group_type&		   group_;
		read_accessor	   read_accessor_;

		[[nodiscard]] auto propagate_rep() const noexcept -> const group_type& { return group_; }

		[[nodiscard]] auto propagate_rep() noexcept -> group_type& { return group_; }

	public:
		explicit GroupAccessorReadModify(const string_view_type name, group_type& group)
			: group_{group},
			  read_accessor_{name, group_} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] auto name() const noexcept -> string_view_type { return read_accessor_.name(); }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return read_accessor_.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> group_type::size_type { return read_accessor_.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return read_accessor_.contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] auto get(const string_view_type key) const -> string_view_type { return read_accessor_.get(key); }

		/**
		 * @brief Insert a new key-value pair, or do nothing if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(const string_type& key, string_type&& value) -> result_type { return result_type{propagate_rep().try_emplace(key, std::move(value))}; }

		/**
		 * @brief Insert a new key-value pair, or do nothing if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(string_type&& key, string_type&& value) -> result_type { return result_type{propagate_rep().try_emplace(std::move(key), std::move(value))}; }

		/**
		 * @brief Insert a node previously released from the group, or do nothing if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(node_type&& node) -> result_type
		{
			const auto [it, inserted, inserted_node] = propagate_rep().insert(std::move(node));
			return result_type{{it, inserted}};
		}

		/**
		 * @brief Insert a new key-value pair, or assign if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(const string_type& key, string_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(key, std::move(value))}; }

		/**
		 * @brief Insert a new key-value pair, or assign if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(string_type&& key, string_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(std::move(key), std::move(value))}; }

		/**
		 * @brief Insert a node previously released from the group, or assign if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(node_type&& node) -> result_type;

		/**
		 * @brief Remove a key-value pair from a group.
		 * @param key The key of the pair.
		 * @return Whether the removal is successful or not. (If the key does not exist, the removal fails)
		 */
		auto remove(const string_view_type key) -> bool
		{
			return table_modifier::erase(propagate_rep(), key);
		}

		/**
		 * @brief Release a node from the group. (After that you can change the key/value of the node and insert it back into the group)
		 * @param key The key of the pair.
		 * @return The node.
		 */
		auto extract(const string_view_type key) -> node_type
		{
			return node_type{table_modifier::extract(propagate_rep(), key)};
		}
	};

	class GroupAccessorReadOnlyWithComment
	{
		friend GroupAccessorReadModifyWithComment;

	public:
		struct variable_with_comment
		{
			comment_type comment;
			string_type	 variable;
			comment_type inline_comment;
		};

		// comment
		// key = value inline_comment
		using variables_type = unordered_table_type<variable_with_comment>;

		struct group_with_comment
		{
			comment_type   comment;
			comment_type   inline_comment;
			variables_type variables;
		};

		using group_type = group_with_comment;

	private:
		string_view_type  name_;
		const group_type& group_;

	public:
		explicit GroupAccessorReadOnlyWithComment(const string_view_type name, const group_type& group)
			: name_{name},
			  group_{group} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] auto name() const noexcept -> string_view_type { return name_; }

		/**
		 * @brief Determine if the group has comments.
		 * @return The group has comments or not.
		 * @example
		 * # this is a comment
		 * [group_name]
		 */
		[[nodiscard]] auto has_comment() const noexcept -> bool { return !group_.comment.empty(); }

		/**
		 * @brief Determine if the group has inline comments.
		 * @return The group has inline comments or not.
		 * @example
		 * [group_name] # this is a inline comment
		 */
		[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return !group_.inline_comment.empty(); }

		/**
		 * @brief Get the group's comment.(If not, return an empty comment)
		 * @return The comment of the group.
		 */
		[[nodiscard]] auto comment() const noexcept -> comment_view_type { return group_.comment; }

		/**
		 * @brief Get the group's inline comment.(If not, return an empty inline comment)
		 * @return The inline comment of the group.
		 */
		[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return group_.inline_comment; }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return group_.variables.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> variables_type::size_type { return group_.variables.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.variables.contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
		{
			if (const auto it = table_finder{}(group_.variables, key);
				it != group_.variables.end()) { return it->second.variable; }
			return {};
		}

		/**
		 * @brief Determine if the key-value pair in the group has a comment.
		 * @param key The key of the pair.
		 * @return Returns true if and only if the group contains the key and it has a comment.
		 */
		[[nodiscard]] auto has_comment(const string_view_type key) const -> bool
		{
			if (const auto it = table_finder{}(group_.variables, key);
				it == group_.variables.end()) { return false; }
			else { return !it->second.comment.empty(); }
		}

		/**
		 * @brief Determine if the key-value pair in the group has a inline comment.
		 * @param key The key of the pair.
		 * @return Returns true if and only if the group contains the key and it has a inline comment.
		 */
		[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool
		{
			if (const auto it = table_finder{}(group_.variables, key);
				it == group_.variables.end()) { return false; }
			else { return !it->second.inline_comment.empty(); }
		}

		/**
		 * @brief Get the comment of a key-value pair in a group.
		 * @param key The key of the pair.
		 * @return Returns not empty comment if and only if the group contains the key and it has a comment.
		 */
		[[nodiscard]] auto comment(const string_view_type key) const -> comment_view_type
		{
			if (const auto it = table_finder{}(group_.variables, key);
				it != group_.variables.end()) { return it->second.comment; }
			return {};
		}

		/**
		 * @brief Get the inline comment of a key-value pair in a group.
		 * @param key The key of the pair.
		 * @return Returns not empty inline comment if and only if the group contains the key and it has a inline comment.
		 */
		[[nodiscard]] auto inline_comment(const string_view_type key) const -> comment_view_type
		{
			if (const auto it = table_finder{}(group_.variables, key);
				it != group_.variables.end()) { return it->second.inline_comment; }
			return {};
		}
	};

	class GroupAccessorReadModifyWithComment
	{
		using read_accessor = GroupAccessorReadOnlyWithComment;

	public:
		using variables_type = read_accessor::variables_type;
		using group_type	 = read_accessor::group_type;

		class Node
		{
			friend GroupAccessorReadModifyWithComment;

		public:
			constexpr static std::size_t max_elements_size = 4;

			template<std::size_t Index>
			// requires(Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					comment_type,
					std::conditional_t<
							Index == 1,
							string_type,
							std::conditional_t<
									Index == 2,
									string_type,
									std::conditional_t<
											Index == 3,
											comment_type,
											void>>>>;

		private:
			variables_type::node_type node_;

			explicit Node(variables_type::node_type&& node)
				: node_{std::move(node)} {}

			explicit(false) operator variables_type::node_type&&() && { return std::move(node_); }

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() const& -> const index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.mapped().comment; }
				else if constexpr (Index == 1) { return node_.key(); }
				else if constexpr (Index == 2) { return node_.mapped().variable; }
				else if constexpr (Index == 3) { return node_.mapped().inline_comment; }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() & -> index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.mapped().comment; }
				else if constexpr (Index == 1) { return node_.key(); }
				else if constexpr (Index == 2) { return node_.mapped().variable; }
				else if constexpr (Index == 3) { return node_.mapped().inline_comment; }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() && -> index_type<Index>&&
			{
				if constexpr (Index == 0) { return std::move(node_.mapped().comment); }
				else if constexpr (Index == 1) { return std::move(node_.key()); }
				else if constexpr (Index == 2) { return std::move(node_.mapped().variable); }
				else if constexpr (Index == 3) { return std::move(node_.mapped().inline_comment); }
				else { GAL_INI_UNREACHABLE(); }
			}

			/**
			 * @brief Get the node's comment.
			 * @return The comment of the node.
			 */
			[[nodiscard]] auto	   comment() const& -> comment_view_type { return get<0>(); }

			/**
			 * @brief Get the node's comment.
			 * @return The comment of the node.
			 */
			[[nodiscard]] auto	   comment() & -> comment_view_type { return get<0>(); }

			/**
			 * @brief Get the node's comment.
			 * @return The comment of the node.
			 */
			[[nodiscard]] auto	   comment() && -> comment_type&& { return std::move(*this).get<0>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() const& -> string_view_type { return get<1>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() & -> string_view_type { return get<1>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] auto	   key() && -> string_type&& { return std::move(*this).get<1>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() const& -> string_view_type { return get<2>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() & -> string_view_type { return get<2>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] auto	   value() && -> string_type&& { return std::move(*this).get<2>(); }

			/**
			 * @brief Get the node's inline comment.
			 * @return The inline comment of the node.
			 */
			[[nodiscard]] auto	   inline_comment() const& -> comment_view_type { return get<3>(); }

			/**
			 * @brief Get the node's inline comment.
			 * @return The inline comment of the node.
			 */
			[[nodiscard]] auto	   inline_comment() & -> comment_view_type { return get<3>(); }

			/**
			 * @brief Get the node's inline comment.
			 * @return The inline comment of the node.
			 */
			[[nodiscard]] auto	   inline_comment() && -> comment_type&& { return std::move(*this).get<3>(); }

			/**
			 * @brief Determine if the node is valid (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] explicit operator bool() const noexcept { return node_.operator bool(); }

			/**
			 * @brief Determine if the node is valid (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] auto	   empty() const noexcept -> bool { return node_.empty(); }
		};

		using node_type = Node;

		class InsertResult
		{
			friend GroupAccessorReadModifyWithComment;

		public:
			constexpr static std::size_t max_elements_size = node_type::max_elements_size + 1;

			template<std::size_t Index>
				requires(Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					bool,
					node_type::index_type<Index - 1>>;

		private:
			using result_type = std::pair<variables_type::const_iterator, bool>;

			result_type result_;

			explicit InsertResult(result_type result)
				: result_{result} {}

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] auto get() const -> const index_type<Index>&
			{
				if constexpr (Index == 0) { return result_.second; }
				else if constexpr (Index == 1) { return result_.first->second.comment; }
				else if constexpr (Index == 2) { return result_.first->first; }
				else if constexpr (Index == 3) { return result_.first->second.variable; }
				else if constexpr (Index == 4) { return result_.first->second.inline_comment; }
				else { GAL_INI_UNREACHABLE(); }
			}

			/**
			 * @brief Determine if the insertion was successful.
			 * @return The insertion was successes or not.
			 * @note If it overwrites an existing value, it is considered not inserted.
			 */
			[[nodiscard]] explicit operator bool() const noexcept { return result_.second; }

			/**
			 * @brief Determine if the insertion was successful.
			 * @return The insertion was successes or not.
			 * @note If it overwrites an existing value, it is considered not inserted.
			 */
			[[nodiscard]] auto	   result() const -> bool { return get<0>(); }

			/**
			 * @brief Get the inserted comment.
			 * @return The comment of the insertion.
			 */
			[[nodiscard]] auto	   comment() const -> comment_view_type { return get<1>(); }

			/**
			 * @brief Get the inserted key.
			 * @return The key of the insertion.
			 */
			[[nodiscard]] auto	   key() const -> string_view_type { return get<2>(); }

			/**
			 * @brief Get the inserted value.
			 * @return The value of the insertion.
			 */
			[[nodiscard]] auto	   value() const -> string_view_type { return get<3>(); }

			/**
			 * @brief Get the inserted inline comment.
			 * @return The inline comment of the insertion.
			 */
			[[nodiscard]] auto	   inline_comment() const -> comment_view_type { return get<4>(); }
		};

		using result_type = InsertResult;

	private:
		group_type&		   group_;
		read_accessor	   read_accessor_;

		[[nodiscard]] auto propagate_rep() const noexcept -> const group_type& { return group_; }

		[[nodiscard]] auto propagate_rep() noexcept -> group_type& { return group_; }

	public:
		explicit GroupAccessorReadModifyWithComment(const string_view_type name, group_type& group)
			: group_{group},
			  read_accessor_{name, group_} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] auto name() const noexcept -> string_view_type { return read_accessor_.name(); }

		/**
		 * @brief Determine if the group has comments.
		 * @return The group has comments or not.
		 * @example
		 * # this is a comment
		 * [group_name]
		 */
		[[nodiscard]] auto has_comment() const noexcept -> bool { return read_accessor_.has_comment(); }

		/**
		 * @brief Determine if the group has inline comments.
		 * @return The group has inline comments or not.
		 * @example
		 * [group_name] # this is a inline comment
		 */
		[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return read_accessor_.has_inline_comment(); }

		/**
		 * @brief Get the group's comment.(If not, return an empty comment)
		 * @return The comment of the group.
		 */
		[[nodiscard]] auto comment() const noexcept -> comment_view_type { return read_accessor_.comment(); }

		/**
		 * @brief Get the group's inline comment.(If not, return an empty inline comment)
		 * @return The inline comment of the group.
		 */
		[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return read_accessor_.inline_comment(); }

		/**
		 * @brief Set the comment for this group.
		 * @param comment The comment to set.
		 */
		auto			   comment(comment_type&& comment) -> void { propagate_rep().comment = std::move(comment); }

		/**
		 * @brief Set the inline comment for this group.
		 * @param inline_comment The inline comment to set.
		 */
		auto			   inline_comment(comment_type&& inline_comment) -> void { propagate_rep().inline_comment = std::move(inline_comment); }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return read_accessor_.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> variables_type ::size_type { return read_accessor_.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return read_accessor_.contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] auto get(const string_view_type key) const -> string_view_type { return read_accessor_.get(key); }

		/**
		 * @brief Determine if the key-value pair in the group has a comment.
		 * @param key The key of the pair.
		 * @return Returns true if and only if the group contains the key and it has a comment.
		 */
		[[nodiscard]] auto has_comment(const string_view_type key) const -> bool { return read_accessor_.has_comment(key); }

		/**
		 * @brief Determine if the key-value pair in the group has a inline comment.
		 * @param key The key of the pair.
		 * @return Returns true if and only if the group contains the key and it has a inline comment.
		 */
		[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool { return read_accessor_.has_inline_comment(key); }

		/**
		 * @brief Get the comment of a key-value pair in a group.
		 * @param key The key of the pair.
		 * @return Returns not empty comment if and only if the group contains the key and it has a comment.
		 */
		[[nodiscard]] auto comment(const string_view_type key) const -> comment_view_type { return read_accessor_.comment(key); }

		/**
		 * @brief Get the inline comment of a key-value pair in a group.
		 * @param key The key of the pair.
		 * @return Returns not empty inline comment if and only if the group contains the key and it has a inline comment.
		 */
		[[nodiscard]] auto inline_comment(const string_view_type key) const -> comment_view_type { return read_accessor_.inline_comment(key); }

		/**
		 * @brief Insert a new key-value pair with optional comment and inline comment, or do nothing if it already exist.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @param comment Comment to insert.
		 * @param inline_comment Inline comment to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().variables.try_emplace(key, read_accessor ::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

		/**
		 * @brief Insert a new key-value pair with optional comment and inline comment, or do nothing if it already exist.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @param comment Comment to insert.
		 * @param inline_comment Inline comment to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().variables.try_emplace(std::move(key), read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

		/**
		 * @brief Insert a node previously released from the group, or do nothing if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		auto			   try_insert(node_type&& node) -> result_type
		{
			const auto [it, inserted, inserted_node] = propagate_rep().variables.insert(std::move(node));
			return result_type{{it, inserted}};
		}

		/**
		 * @brief Insert a new key-value pair with optional comment and inline comment, or assign if it already exist.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @param comment Comment to insert.
		 * @param inline_comment Inline comment to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().variables.insert_or_assign(key, read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

		/**
		 * @brief Insert a new key-value pair with optional comment and inline comment, or assign if it already exist.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @param comment Comment to insert.
		 * @param inline_comment Inline comment to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().variables.insert_or_assign(std::move(key), read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

		/**
		 * @brief Insert a node previously released from the group, or assign if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		auto insert_or_assign(node_type&& node) -> result_type;

		/**
		 * @brief Remove a key-value pair from a group.
		 * @param key The key of the pair.
		 * @return Whether the removal is successful or not. (If the key does not exist, the removal fails)
		 */
		auto remove(const string_view_type key) -> bool { return table_modifier::erase(propagate_rep().variables, key); }

		/**
		 * @brief Release a node from the group. (After that you can change the key/value of the node and insert it back into the group)
		 * @param key The key of the pair.
		 * @return The node.
		 */
		auto extract(const string_view_type key) -> node_type { return node_type{table_modifier::extract(propagate_rep().variables, key)}; }
	};

	class GroupAccessorWriteOnly
	{
	public:
		using group_type = unordered_table_view_type<string_view_type>;

	private:
		group_type group_;

	public:
		explicit GroupAccessorWriteOnly(const GroupAccessorReadOnly::group_type& group);

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return group_.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.contains(key); }

		/**
		 * @brief Write the key-value pair corresponding to the key into out. (Or do nothing if it not exist).
		 * @param key The key of the pair.
		 * @param out The destination.
		 * @note This does not write line_separator, as it can not determine if there are trailing inline comment.
		 */
		template<typename Out>
			requires requires(Out& out) {
						 out << std::declval<string_view_type>() << kv_separator << line_separator;
					 }
		auto flush(string_view_type key, Out& out) -> Out&
		{
			if (const auto it = group_.find(key);
				it != group_.end())
			{
				out << it->first << kv_separator << it->second;
				group_.erase(it);
			}
			return out;
		}

		/**
		 * @brief Write all remaining key-value pairs to out.(no longer considers if there are comments)
		 * @param out The destination.
		 */
		template<typename Out>
			requires requires(Out& out) {
						 out << std::declval<string_view_type>() << kv_separator << line_separator;
					 }
		auto flush_remainder(Out& out) -> Out&
		{
			for (const auto& [key, value]: group_) { out << key << kv_separator << value << line_separator; }
			group_.clear();
			return out;
		}
	};

	class GroupAccessorWriteOnlyWithComment
	{
	public:
		struct variable_with_comment
		{
			comment_view_type comment;
			string_view_type  variable;
			comment_view_type inline_comment;
		};

		using variables_type = unordered_table_view_type<variable_with_comment>;

		struct group_with_comment
		{
			comment_view_type comment;
			comment_view_type inline_comment;
			variables_type	  variables;
		};

		using group_type = group_with_comment;

	private:
		group_type group_;

	public:
		explicit GroupAccessorWriteOnlyWithComment(const GroupAccessorReadOnlyWithComment::group_type& group);

		/**
		 * @brief Determine if the group has comments.
		 * @return The group has comments or not.
		 * @example
		 * # this is a comment
		 * [group_name]
		 */
		[[nodiscard]] auto has_comment() const noexcept -> bool { return !group_.comment.empty(); }

		/**
		 * @brief Determine if the group has inline comments.
		 * @return The group has inline comments or not.
		 * @example
		 * [group_name] # this is a inline comment
		 */
		[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return !group_.inline_comment.empty(); }

		/**
		 * @brief Get the group's comment.(If not, return an empty comment)
		 * @return The comment of the group.
		 */
		[[nodiscard]] auto comment() const noexcept -> comment_view_type { return group_.comment; }

		/**
		 * @brief Get the group's inline comment.(If not, return an empty inline comment)
		 * @return The inline comment of the group.
		 */
		[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return group_.inline_comment; }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] auto empty() const noexcept -> bool { return group_.variables.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] auto size() const noexcept -> variables_type ::size_type { return group_.variables.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.variables.contains(key); }

		/**
		 * @brief Write the key-value pair, comment and inline comment corresponding to the key into out. (Or do nothing if it not exist).
		 * @param key The key of the pair.
		 * @param out The destination.
		 * @note The group knows the value/comment/inline comment corresponding to the key, so it writes everything in its entirety, including line_separator.
		 */
		template<typename Out>
			requires requires(Out& out) {
						 out << std::declval<string_view_type>() << kv_separator << line_separator;
					 }
		auto flush(string_view_type key, Out& out) -> Out&
		{
			if (const auto it = group_.variables.find(key);
				it != group_.variables.end())
			{
				if (!it->second.comment.empty())
				{
					const auto& [indication, comment] = it->second.comment;
					out << make_comment_indication(indication) << blank_separator << comment << line_separator;
				}

				out << it->first << kv_separator << it->second.variable;
				if (!it->second.inline_comment.empty())
				{
					const auto& [indication, comment] = it->second.inline_comment;
					out << blank_separator << make_comment_indication(indication) << blank_separator << comment;
				}
				out << line_separator;

				group_.variables.erase(it);
			}
			return out;
		}

		/**
		 * @brief Write all remaining key-value pairs to out.(no longer considers if there are comments)
		 * @param out The destination.
		 */
		template<typename Out>
			requires requires(Out& out) {
						 out << std::declval<string_view_type>() << kv_separator << line_separator;
					 }
		auto flush_remainder(Out& out) -> Out&
		{
			for (const auto& [key, variable_with_comment]: group_.variables)
			{
				if (!variable_with_comment.comment.empty())
				{
					const auto& [indication, comment] = variable_with_comment.comment;
					out << make_comment_indication(indication) << blank_separator << comment << line_separator;
				}

				out << key << kv_separator << variable_with_comment.variable;
				if (!variable_with_comment.inline_comment.empty())
				{
					const auto& [indication, comment] = variable_with_comment.inline_comment;
					out << blank_separator << make_comment_indication(indication) << blank_separator << comment;
				}
				out << line_separator;
			}
			group_.variables.clear();
			return out;
		}
	};
}// namespace gal::ini::impl

// ===========================
// READ MODIFY
// ===========================

template<>
struct std::tuple_size<gal::ini::impl::GroupAccessorReadModify::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::GroupAccessorReadModify::node_type::max_elements_size;
};

template<>
struct std::tuple_size<gal::ini::impl::GroupAccessorReadModify::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::GroupAccessorReadModify::result_type::max_elements_size;
};

template<std::size_t Index>
struct std::tuple_element<Index, gal::ini::impl::GroupAccessorReadModify::node_type>
{
	using type = gal::ini::impl::GroupAccessorReadModify::node_type::index_type<Index>;
};

template<std::size_t Index>
struct std::tuple_element<Index, gal::ini::impl::GroupAccessorReadModify::result_type>
{
	using type = gal::ini::impl::GroupAccessorReadModify::result_type::index_type<Index>;
};

// ===========================
// READ MODIFY WITH COMMENT
// ===========================

template<>
struct std::tuple_size<gal::ini::impl::GroupAccessorReadModifyWithComment::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::GroupAccessorReadModifyWithComment::node_type::max_elements_size;
};

template<>
struct std::tuple_size<gal::ini::impl::GroupAccessorReadModifyWithComment::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::GroupAccessorReadModifyWithComment::result_type::max_elements_size;
};

template<std::size_t Index>
struct std::tuple_element<Index, gal::ini::impl::GroupAccessorReadModifyWithComment::node_type>
{
	using type = gal::ini::impl::GroupAccessorReadModifyWithComment::node_type::index_type<Index>;
};

template<std::size_t Index>
struct std::tuple_element<Index, gal::ini::impl::GroupAccessorReadModifyWithComment::result_type>
{
	using type = gal::ini::impl::GroupAccessorReadModifyWithComment::result_type::index_type<Index>;
};

namespace gal::ini::impl
{
	inline auto GroupAccessorReadModify::insert_or_assign(node_type&& node) -> result_type
	{
		auto&& [key, value] = std::move(node);
		return insert_or_assign(std::move(key), std::move(value));
	}

	inline auto GroupAccessorReadModifyWithComment::insert_or_assign(node_type&& node) -> result_type
	{
		auto&& [comment, key, value, inline_comment] = std::move(node);
		return insert_or_assign(std::move(key), std::move(value), std::move(comment), std::move(inline_comment));
	}
}// namespace gal::ini::impl
