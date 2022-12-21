#pragma once

#include <ranges>

namespace gal::ini::impl
{
	class IniParser;
	class IniParserWithComment;

	enum class GroupProperty
	{
		READ_ONLY,
		READ_MODIFY,

		READ_ONLY_WITH_COMMENT,
		READ_MODIFY_WITH_COMMENT,

		WRITE_ONLY,
		WRITE_ONLY_WITH_COMMENT,
	};

	template<typename Map>
	struct map_transparent_find_invoker
	{
		[[nodiscard]] static auto do_find(Map& map, const string_view_type key) -> typename Map::iterator { return map.find(string_type{key}); }
		[[nodiscard]] static auto do_find(const Map& map, const string_view_type key) -> typename Map::const_iterator { return map.find(string_type{key}); }
	};

	template<typename Map>
		requires requires
		{
			{
				std::declval<Map&>().find(std::declval<const string_view_type>())
			} -> std::same_as<typename Map::iterator>;

			{
				std::declval<const Map&>().find(std::declval<const string_view_type>())
			} -> std::same_as<typename Map::const_iterator>;
		}
	struct map_transparent_find_invoker<Map>
	{
		[[nodiscard]] static auto do_find(Map& map, const string_view_type key) -> typename Map::iterator { return map.find(key); }
		[[nodiscard]] static auto do_find(const Map& map, const string_view_type key) -> typename Map::const_iterator { return map.find(key); }
	};

	template<typename Map>
	struct map_transparent_modifier
	{
		static auto do_erase(Map& map, const string_view_type key) { return map.erase(string_type{key}); }

		static auto do_extract(Map& map, const string_view_type key) -> typename Map::node_type { return map.extract(string_type{key}); }
	};

	template<typename Map>
		requires requires
		{
			std::declval<Map&>().remove(std::declval<const string_view_type>());

			{
				std::declval<Map&>().extract(std::declval<const string_view_type>())
			} -> std::same_as<typename Map::node_type>;
		}
	struct map_transparent_modifier<Map>
	{
		// cxx23 required
		static auto do_erase(Map& map, const string_view_type key) { return map.erase(key); }

		static auto do_extract(Map& map, const string_view_type key) -> typename Map::node_type { return map.extract(key); }
	};

	namespace detail
	{
		template<GroupProperty>
		class GroupAccessor;

		template<>
		class GroupAccessor<GroupProperty::READ_ONLY>
		{
			friend IniParser;
			friend GroupAccessor<GroupProperty::READ_MODIFY>;

		public:
			using group_type = unordered_table_type<string_type>;

		private:
			const group_type& group_;
			string_view_type  name_;

			explicit GroupAccessor(const group_type& group, const string_view_type name)
				: group_{group},
				name_{name} {}

		public:
			[[nodiscard]] auto name() const noexcept -> string_view_type { return name_; }

			[[nodiscard]] auto empty() const noexcept -> bool { return group_.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.contains(key); }

			[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_, key);
					it != group_.end()) { return it->second; }

				return {};
			}
		};

		template<>
		class GroupAccessor<GroupProperty::READ_MODIFY>
		{
			friend IniParser;

			using read_accessor_type = GroupAccessor<GroupProperty::READ_ONLY>;

		public:
			using group_type = read_accessor_type::group_type;

			class Node
			{
				friend GroupAccessor;

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
				[[nodiscard]] auto get() const & -> const index_type<Index>&
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

				[[nodiscard]] auto key() const & -> string_view_type { return get<0>(); }

				[[nodiscard]] auto key() & -> string_view_type { return get<0>(); }

				[[nodiscard]] auto key() && -> string_type&& { return std::move(*this).get<0>(); }

				[[nodiscard]] auto value() const & -> string_view_type { return get<1>(); }

				[[nodiscard]] auto value() & -> string_view_type { return get<1>(); }

				[[nodiscard]] auto value() && -> string_type&& { return std::move(*this).get<1>(); }

				[[nodiscard]] explicit operator bool() const noexcept { return node_.operator bool(); }

				[[nodiscard]] auto empty() const noexcept -> bool { return node_.empty(); }
			};

			using node_type = Node;

			class InsertResult
			{
				friend GroupAccessor;

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

				[[nodiscard]] explicit operator bool() const noexcept { return result_.second; }

				[[nodiscard]] auto result() const -> bool { return get<0>(); }

				[[nodiscard]] auto key() const -> string_view_type { return get<1>(); }

				[[nodiscard]] auto value() const -> string_view_type { return get<2>(); }
			};

			using result_type = InsertResult;

		private:
			group_type&        group_;
			read_accessor_type read_accessor_;

			explicit GroupAccessor(group_type& group, const string_view_type name)
				: group_{group},
				read_accessor_{group_, name} {}

			[[nodiscard]] auto propagate_rep() const noexcept -> const group_type& { return group_; }

			[[nodiscard]] auto propagate_rep() noexcept -> group_type& { return group_; }

		public:
			[[nodiscard]] auto name() const noexcept -> string_view_type { return read_accessor_.name(); }

			[[nodiscard]] auto empty() const noexcept -> bool { return read_accessor_.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return read_accessor_.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return read_accessor_.contains(key); }

			[[nodiscard]] auto get(const string_view_type key) const -> string_view_type { return read_accessor_.get(key); }

			auto try_insert(const string_type& key, string_type&& value) -> result_type { return result_type{propagate_rep().try_emplace(key, std::move(value))}; }

			auto try_insert(string_type&& key, string_type&& value) -> result_type { return result_type{propagate_rep().try_emplace(std::move(key), std::move(value))}; }

			auto try_insert(node_type&& node) -> result_type
			{
				const auto [it, inserted, inserted_node] = propagate_rep().insert(std::move(node));
				return result_type{{it, inserted}};
			}

			auto insert_or_assign(const string_type& key, string_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(key, std::move(value))}; }

			auto insert_or_assign(string_type&& key, string_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(std::move(key), std::move(value))}; }

			auto insert_or_assign(node_type&& node) -> result_type;

			auto remove(const string_view_type key) -> bool { return map_transparent_modifier<group_type>::do_erase(propagate_rep(), key); }

			auto extract(const string_view_type key) -> node_type { return node_type{map_transparent_modifier<group_type>::do_extract(propagate_rep(), key)}; }
		};

		template<>
		class GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>
		{
			friend IniParserWithComment;
			friend GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;

		public:
			struct variable_with_comment
			{
				comment_type comment;
				string_type  variable;
				comment_type inline_comment;
			};

			// comment
			// key = value inline_comment
			using group_type = unordered_table_type<variable_with_comment>;

			struct group_with_comment_type
			{
				comment_type comment;
				comment_type inline_comment;
				group_type   group;
			};

		private:
			const group_with_comment_type& group_;
			string_view_type               name_;

			explicit GroupAccessor(const group_with_comment_type& group, const string_view_type name)
				: group_{group},
				name_{name} {}

		public:
			[[nodiscard]] auto name() const noexcept -> string_view_type { return name_; }

			[[nodiscard]] auto has_comment() const noexcept -> bool { return !group_.comment.empty(); }

			[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return !group_.inline_comment.empty(); }

			[[nodiscard]] auto comment() const noexcept -> comment_view_type { return group_.comment; }

			[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return group_.inline_comment; }

			[[nodiscard]] auto empty() const noexcept -> bool { return group_.group.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.group.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.group.contains(key); }

			[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_.group, key);
					it != group_.group.end()) { return it->second.variable; }
				return {};
			}

			[[nodiscard]] auto has_comment(const string_view_type key) const -> bool
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_.group, key);
					it == group_.group.end()) { return false; }
				else { return !it->second.comment.empty(); }
			}

			[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_.group, key);
					it == group_.group.end()) { return false; }
				else { return !it->second.inline_comment.empty(); }
			}

			[[nodiscard]] auto comment(const string_view_type key) const -> comment_view_type
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_.group, key);
					it != group_.group.end()) { return it->second.comment; }
				return {};
			}

			[[nodiscard]] auto inline_comment(const string_view_type key) const -> comment_view_type
			{
				if (const auto it = map_transparent_find_invoker<group_type>::do_find(group_.group, key);
					it != group_.group.end()) { return it->second.inline_comment; }
				return {};
			}
		};

		template<>
		class GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>
		{
			friend IniParserWithComment;

			using read_accessor_type = GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>;

		public:
			using group_type = read_accessor_type::group_type;
			using group_with_comment_type = read_accessor_type::group_with_comment_type;

			class Node
			{
				friend GroupAccessor;

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
				group_type::node_type node_;

				explicit Node(group_type::node_type&& node)
					: node_{std::move(node)} {}

				explicit(false) operator group_type::node_type&&() && { return std::move(node_); }

			public:
				template<std::size_t Index>
					requires(Index < max_elements_size)
				[[nodiscard]] auto get() const & -> const index_type<Index>&
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

				[[nodiscard]] auto comment() const & -> comment_view_type { return get<0>(); }

				[[nodiscard]] auto comment() & -> comment_view_type { return get<0>(); }

				[[nodiscard]] auto comment() && -> comment_type&& { return std::move(*this).get<0>(); }

				[[nodiscard]] auto key() const & -> string_view_type { return get<1>(); }

				[[nodiscard]] auto key() & -> string_view_type { return get<1>(); }

				[[nodiscard]] auto key() && -> string_type&& { return std::move(*this).get<1>(); }

				[[nodiscard]] auto value() const & -> string_view_type { return get<2>(); }

				[[nodiscard]] auto value() & -> string_view_type { return get<2>(); }

				[[nodiscard]] auto value() && -> string_type&& { return std::move(*this).get<2>(); }

				[[nodiscard]] auto inline_comment() const & -> comment_view_type { return get<3>(); }

				[[nodiscard]] auto inline_comment() & -> comment_view_type { return get<3>(); }

				[[nodiscard]] auto inline_comment() && -> comment_type&& { return std::move(*this).get<3>(); }

				[[nodiscard]] explicit operator bool() const noexcept { return node_.operator bool(); }

				[[nodiscard]] auto empty() const noexcept -> bool { return node_.empty(); }
			};

			using node_type = Node;

			class InsertResult
			{
				friend GroupAccessor;

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
					else if constexpr (Index == 1) { return result_.first->second.comment; }
					else if constexpr (Index == 2) { return result_.first->first; }
					else if constexpr (Index == 3) { return result_.first->second.variable; }
					else if constexpr (Index == 4) { return result_.first->second.inline_comment; }
					else { GAL_INI_UNREACHABLE(); }
				}

				[[nodiscard]] explicit operator bool() const noexcept { return result_.second; }

				[[nodiscard]] auto result() const -> bool { return get<0>(); }

				[[nodiscard]] auto comment() const -> comment_view_type { return get<1>(); }

				[[nodiscard]] auto key() const -> string_view_type { return get<2>(); }

				[[nodiscard]] auto value() const -> string_view_type { return get<3>(); }

				[[nodiscard]] auto inline_comment() const -> comment_view_type { return get<4>(); }
			};

			using result_type = InsertResult;

		private:
			group_with_comment_type& group_;
			read_accessor_type       read_accessor_;

			explicit GroupAccessor(group_with_comment_type& group, const string_view_type name)
				: group_{group},
				read_accessor_{group_, name} {}

			[[nodiscard]] auto propagate_rep() const noexcept -> const group_with_comment_type& { return group_; }

			[[nodiscard]] auto propagate_rep() noexcept -> group_with_comment_type& { return group_; }

		public:
			[[nodiscard]] auto name() const noexcept -> string_view_type { return read_accessor_.name(); }

			[[nodiscard]] auto has_comment() const noexcept -> bool { return read_accessor_.has_comment(); }

			[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return read_accessor_.has_inline_comment(); }

			[[nodiscard]] auto comment() const noexcept -> comment_view_type { return read_accessor_.comment(); }

			[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return read_accessor_.inline_comment(); }

			auto comment(comment_type&& comment) -> void { propagate_rep().comment = std::move(comment); }

			auto inline_comment(comment_type&& inline_comment) -> void { propagate_rep().inline_comment = std::move(inline_comment); }

			[[nodiscard]] auto empty() const noexcept -> bool { return read_accessor_.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return read_accessor_.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return read_accessor_.contains(key); }

			[[nodiscard]] auto get(const string_view_type key) const -> string_view_type { return read_accessor_.get(key); }

			[[nodiscard]] auto has_comment(const string_view_type key) const -> bool { return read_accessor_.has_comment(key); }

			[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool { return read_accessor_.has_inline_comment(key); }

			[[nodiscard]] auto comment(const string_view_type key) const -> comment_view_type { return read_accessor_.comment(key); }

			[[nodiscard]] auto inline_comment(const string_view_type key) const -> comment_view_type { return read_accessor_.inline_comment(key); }

			auto try_insert(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().group.try_emplace(key, read_accessor_type::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

			auto try_insert(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().group.try_emplace(std::move(key), read_accessor_type::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

			auto try_insert(node_type&& node) -> result_type
			{
				const auto [it, inserted, inserted_node] = propagate_rep().group.insert(std::move(node));
				return result_type{{it, inserted}};
			}

			auto insert_or_assign(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().group.insert_or_assign(key, read_accessor_type::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

			auto insert_or_assign(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type { return result_type{propagate_rep().group.insert_or_assign(std::move(key), read_accessor_type::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})}; }

			auto insert_or_assign(node_type&& node) -> result_type;

			auto remove(const string_view_type key) -> bool { return map_transparent_modifier<group_type>::do_erase(propagate_rep().group, key); }

			auto extract(const string_view_type key) -> node_type { return node_type{map_transparent_modifier<group_type>::do_extract(propagate_rep().group, key)}; }
		};

		template<>
		class GroupAccessor<GroupProperty::WRITE_ONLY>
		{
			friend IniParser;

		public:
			using group_type = unordered_map_type<string_view_type, string_view_type, string_hash_type>;

		private:
			group_type group_;

			explicit GroupAccessor(const GroupAccessor<GroupProperty::READ_ONLY>::group_type& group);

		public:
			[[nodiscard]] auto empty() const noexcept -> bool { return group_.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.contains(key); }

			auto flush(string_view_type key, std::ostream& out) -> void;

			auto flush_remainder(std::ostream& out) -> void;
		};

		template<>
		class GroupAccessor<GroupProperty::WRITE_ONLY_WITH_COMMENT>
		{
			friend IniParserWithComment;

		public:
			struct variable_with_comment
			{
				comment_view_type comment;
				string_view_type  variable;
				comment_view_type inline_comment;
			};

			using group_type = unordered_map_type<string_view_type, variable_with_comment, string_hash_type>;

			struct group_with_comment_type
			{
				comment_view_type comment;
				comment_view_type inline_comment;
				group_type        group;
			};

		private:
			group_with_comment_type group_;

			explicit GroupAccessor(const GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>::group_with_comment_type& group);

		public:
			[[nodiscard]] auto has_comment() const noexcept -> bool { return !group_.comment.empty(); }

			[[nodiscard]] auto has_inline_comment() const noexcept -> bool { return !group_.inline_comment.empty(); }

			[[nodiscard]] auto comment() const noexcept -> comment_view_type { return group_.comment; }

			[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type { return group_.inline_comment; }

			[[nodiscard]] auto empty() const noexcept -> bool { return group_.group.empty(); }

			[[nodiscard]] auto size() const noexcept -> group_type::size_type { return group_.group.size(); }

			[[nodiscard]] auto contains(const string_view_type key) const -> bool { return group_.group.contains(key); }

			auto flush(string_view_type key, std::ostream& out) -> void;

			auto flush_remainder(std::ostream& out) -> void;
		};
	}// namespace detail

	class IniParser
	{
		template<typename Ini, bool KeepComment>
		friend class FlushState;

	public:
		using reader_type = detail::GroupAccessor<GroupProperty::READ_ONLY>;
		using writer_type = detail::GroupAccessor<GroupProperty::READ_MODIFY>;
		using flush_type = detail::GroupAccessor<GroupProperty::WRITE_ONLY>;

		// key = value
		using group_type = reader_type::group_type;
		// [group_name]
		// key1 = value1
		// key2 = value2
		// key3 = value3
		// ...
		using context_type = unordered_table_type<group_type>;

	private:
		context_type   context_;
		file_path_type file_path_;

	public:
		explicit IniParser(file_path_type&& file_path);

		explicit IniParser(const file_path_type& file_path)
			: IniParser{file_path_type{file_path}} {}

		[[nodiscard]] auto file_path() const noexcept -> const file_path_type& { return file_path_; }

		[[nodiscard]] auto empty() const noexcept -> bool { return context_.empty(); }

		[[nodiscard]] auto size() const noexcept -> context_type::size_type { return context_.size(); }

		[[nodiscard]] auto contains(const string_view_type group_name) const -> bool { return context_.contains(group_name); }

		[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return reader_type{it->second, it->first}; }

			// todo
			const static group_type empty_group{};
			return reader_type{empty_group, "group_not_exist"};
		}

		[[nodiscard]] auto read(const string_view_type group_name) -> reader_type
		{
			if (const auto it =
						#ifdef GAL_INI_COMPILER_GNU
							context_.find(string_type{group_name});
						#else
						context_.find(group_name);
				#endif
				it != context_.end()) { return reader_type{it->second, it->first}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return reader_type{result->second, result->first};
		}

		[[nodiscard]] auto read(string_type&& group_name) -> reader_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return reader_type{it->second, it->first}; }

			const auto result = context_.emplace(std::move(group_name), group_type{}).first;
			return reader_type{result->second, result->first};
		}

		[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return writer_type{it->second, it->first}; }

			const auto result = context_.emplace(group_name, group_type{}).first;
			return writer_type{result->second, result->first};
		}

		[[nodiscard]] auto write(string_type&& group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->second, it->first}; }

			const auto result = context_.emplace(std::move(group_name), group_type{}).first;
			return writer_type{result->second, result->first};
		}

	private:
		auto flush(const string_view_type group_name) -> flush_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return flush_type{it->second}; }

			return flush_type{{}};
		}

	public:
		auto flush(bool keep_comments) -> void;

		template<typename OStream>
		auto print(
				OStream&               out,
				const string_view_type separator = line_separator) const -> void requires
			requires
			{
				out << separator.data();
				out << string_type{};
			}
		{
			for (const auto& [group_name, variables]: context_)
			{
				out << "[" << group_name << "]" << separator.data();
				for (const auto& [variable_key, variable_value]: variables) { out << variable_key << "=" << variable_value << separator.data(); }
			}
		}
	};

	class IniParserWithComment
	{
		template<typename Ini>
		friend class FlushStateWithComment;

	public:
		using reader_type = detail::GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>;
		using writer_type = detail::GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;
		using flush_type = detail::GroupAccessor<GroupProperty::WRITE_ONLY_WITH_COMMENT>;

		using group_type = reader_type::group_type;
		using group_with_comment_type = reader_type::group_with_comment_type;

		// comment
		// [group_name] inline_comment
		// key1 = value1
		// key2 = value2
		// key3 = value3
		// ...
		using context_type = unordered_table_type<group_with_comment_type>;

	private:
		context_type   context_;
		file_path_type file_path_;

	public:
		explicit IniParserWithComment(file_path_type&& file_path);

		explicit IniParserWithComment(const file_path_type& file_path)
			: IniParserWithComment{file_path_type{file_path}} {}

		[[nodiscard]] auto file_path() const noexcept -> const file_path_type& { return file_path_; }

		[[nodiscard]] auto empty() const noexcept -> bool { return context_.empty(); }

		[[nodiscard]] auto size() const noexcept -> context_type::size_type { return context_.size(); }

		[[nodiscard]] auto contains(const string_view_type group_name) const -> bool { return context_.contains(group_name); }

		[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return reader_type{it->second, it->first}; }

			// todo
			const static group_with_comment_type empty_group{};
			return reader_type{empty_group, {}};
		}

		[[nodiscard]] auto read(const string_view_type group_name) -> reader_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return reader_type{it->second, it->first}; }

			const auto result = context_.emplace(group_name, group_with_comment_type{}).first;
			return reader_type{result->second, result->first};
		}

		[[nodiscard]] auto read(string_type&& group_name) -> reader_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return reader_type{it->second, it->first}; }

			const auto result = context_.emplace(std::move(group_name), group_with_comment_type{}).first;
			return reader_type{result->second, result->first};
		}

		[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return writer_type{it->second, it->first}; }

			const auto result = context_.emplace(group_name, group_with_comment_type{}).first;
			return writer_type{result->second, result->first};
		}

		[[nodiscard]] auto write(string_type&& group_name) -> writer_type
		{
			if (const auto it = context_.find(group_name);
				it != context_.end()) { return writer_type{it->second, it->first}; }

			const auto result = context_.emplace(std::move(group_name), group_with_comment_type{}).first;
			return writer_type{result->second, result->first};
		}

	private:
		auto flush(const string_view_type group_name) -> flush_type
		{
			if (const auto it = map_transparent_find_invoker<context_type>::do_find(context_, group_name);
				it != context_.end()) { return flush_type{it->second}; }

			return flush_type{{}};
		}

	public:
		auto flush() -> void;

		template<typename OStream>
		auto print(
				OStream&               out,
				const string_view_type separator = line_separator) const -> void requires requires
		{
			out << separator.data();
			out << string_type{};
		}
		{
			for (const auto& [group_name, group_self]: context_)
			{
				if (!group_self.comment.empty()) { out << group_self.comment.indication << ' ' << group_self.comment.comment << separator.data(); }

				out << "[" << group_name << "]";
				if (!group_self.inline_comment.empty()) { out << ' ' << group_self.inline_comment.indication << ' ' << group_self.inline_comment.comment; }
				out << separator.data();

				for (const auto& [variable_key, variable_with_comment]: group_self.group)
				{
					if (!variable_with_comment.comment.empty()) { out << variable_with_comment.comment.indication << ' ' << variable_with_comment.comment.comment << separator.data(); }

					out << variable_key << "=" << variable_with_comment.variable;
					if (!variable_with_comment.inline_comment.empty()) { out << ' ' << variable_with_comment.inline_comment.indication << ' ' << variable_with_comment.inline_comment.comment; }
					out << separator.data();
				}
			}
		}
	};
}// namespace gal::ini::impl

// ===========================
// READ MODIFY
// ===========================

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::node_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::result_type::max_elements_size;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::node_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::node_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::result_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY>::result_type::index_type<Index>;
};

// ===========================
// READ MODIFY WITH COMMENT
// ===========================

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type::max_elements_size;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::impl::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type::index_type<Index>;
};

namespace gal::ini::impl::detail
{
	inline auto GroupAccessor<GroupProperty::READ_MODIFY>::insert_or_assign(node_type&& node) -> result_type
	{
		auto&& [key, value] = std::move(node);
		return insert_or_assign(std::move(key), std::move(value));
	}

	inline auto GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>::insert_or_assign(node_type&& node) -> result_type
	{
		auto&& [comment, key, value, inline_comment] = std::move(node);
		return insert_or_assign(std::move(key), std::move(value), std::move(comment), std::move(inline_comment));
	}
}
