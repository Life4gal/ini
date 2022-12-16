#pragma once

#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gal::ini
{
#ifndef GAL_INI_STRING_TYPE
	using string_type = std::string;
#else
	using string_type = GAL_INI_STRING_TYPE
#endif

	using char_type			 = string_type::value_type;
	using string_view_type	 = std::basic_string_view<char_type>;

	using filename_type		 = std::string;
	using filename_view_type = std::string_view;

	struct comment_view_type
	{
		char_type					 indication;
		string_view_type			 comment;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool
		{
			return comment.empty();
		}
	};

	struct comment_type
	{
		char_type					 indication;
		string_type					 comment;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool
		{
			return comment.empty();
		}

		[[nodiscard]] constexpr explicit(false) operator comment_view_type() const noexcept
		{
			return {indication, comment};
		}
	};

	enum class GroupProperty
	{
		READ_ONLY,
		READ_MODIFY,

		READ_ONLY_WITH_COMMENT,
		READ_MODIFY_WITH_COMMENT,

		READ_ORDERED,
		READ_MODIFY_ORDERED,

		READ_ORDERED_WITH_COMMENT,
		READ_MODIFY_ORDERED_WITH_COMMENT,
	};

	enum class ParsePolicy
	{
		UNORDERED,
		ORDERED,
	};

	enum class WritePolicy
	{
		SKIP_DUPLICATE,
		OVERWRITE_DUPLICATE,
	};

	namespace impl
	{
		class IniReader;
		class IniReaderWithComment;
		class IniOrderedReader;
		class IniOrderedReaderWithComment;

		namespace detail
		{
			struct hash_type
			{
				using is_transparent = int;

				[[nodiscard]] auto operator()(const string_type& string) const noexcept -> std::size_t
				{
					return std::hash<string_type>{}(string);
				}

				[[nodiscard]] auto operator()(const string_view_type& string) const noexcept -> std::size_t
				{
					return std::hash<string_view_type>{}(string);
				}
			};

			template<typename Char>
			[[nodiscard]] consteval auto line_separator() noexcept
			{
				if constexpr (std::is_same_v<Char, wchar_t>)
				{
#ifdef GAL_INI_COMPILER_MSVC
					return L"\n";
#else
					return L"\r\n";
#endif
				}
				else if constexpr (std::is_same_v<Char, char8_t>)
				{
#ifdef GAL_INI_COMPILER_MSVC
					return u8"\n";
#else
					return u8"\r\n";
#endif
				}
				else if constexpr (std::is_same_v<Char, char16_t>)
				{
#ifdef GAL_INI_COMPILER_MSVC
					return u"\n";
#else
					return u"\r\n";
#endif
				}
				else if constexpr (std::is_same_v<Char, char32_t>)
				{
#ifdef GAL_INI_COMPILER_MSVC
					return U"\n";
#else
					return U"\r\n";
#endif
				}
				else
				{
#ifdef GAL_INI_COMPILER_MSVC
					return "\n";
#else
					return "\r\n";
#endif
				}
			}

			template<GroupProperty>
			class GroupAccessor;

			template<>
			class GroupAccessor<GroupProperty::READ_ONLY>
			{
				friend IniReader;
				friend GroupAccessor<GroupProperty::READ_MODIFY>;

			public:
				using group_type = std::unordered_map<string_type, string_type, detail::hash_type, std::equal_to<>>;

			private:
				const group_type& group_;
				string_view_type  name_;

				explicit GroupAccessor(const group_type& group, const string_view_type name)
					: group_{group},
					  name_{name} {}

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return name_;
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return group_.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return group_.size();
				}

				[[nodiscard]] auto contains(const string_view_type key) const -> bool
				{
					return group_.contains(key);
				}

				[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
				{
					if (const auto it =
					// todo
#ifdef GAL_INI_COMPILER_GNU
								group_.find(string_type{key});
#else
								group_.find(key);
#endif
						it != group_.end())
					{
						return it->second;
					}
					return {};
				}
			};

			template<>
			class GroupAccessor<GroupProperty::READ_MODIFY>
			{
				friend IniReader;

			public:
				using read_accessor = GroupAccessor<GroupProperty::READ_ONLY>;

				using group_type	= read_accessor::group_type;

				class Node
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY>;

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

					explicit Node(group_type::node_type&& node) : node_{std::move(node)} {}

					explicit(false) operator group_type::node_type&&() &&
					{
						return std::move(node_);
					}

				public:
					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() const& -> const index_type<Index>&
					{
						if constexpr (Index == 0)
						{
							return node_.key();
						}
						else if constexpr (Index == 1)
						{
							return node_.mapped();
						}
					}

					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() && -> index_type<Index>&&
					{
						if constexpr (Index == 0)
						{
							return std::move(node_.key());
						}
						else if constexpr (Index == 1)
						{
							return std::move(node_.mapped());
						}
					}

					[[nodiscard]] auto key() const& -> string_view_type
					{
						return get<0>();
					}

					[[nodiscard]] auto key() && -> string_type&&
					{
						return std::move(*this).get<0>();
					}

					[[nodiscard]] auto value() const& -> string_view_type
					{
						return get<1>();
					}

					[[nodiscard]] auto value() && -> string_type&&
					{
						return std::move(*this).get<1>();
					}

					[[nodiscard]] explicit operator bool() const noexcept
					{
						return node_.operator bool();
					}

					[[nodiscard]] auto empty() const noexcept -> bool
					{
						return node_.empty();
					}
				};

				using node_type = Node;

				class InsertResult
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY>;

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

					explicit InsertResult(result_type&& result) : result_{std::move(result)} {}

				public:
					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() const -> const index_type<Index>&
					{
						if constexpr (Index == 0)
						{
							return result_.second;
						}
						else if constexpr (Index == 1)
						{
							return result_.first->first;
						}
						else if constexpr (Index == 2)
						{
							return result_.first->second;
						}
					}

					[[nodiscard]] explicit operator bool() const noexcept
					{
						return result_.second;
					}

					[[nodiscard]] auto result() const -> bool
					{
						return get<0>();
					}

					[[nodiscard]] auto key() const -> string_view_type
					{
						return get<1>();
					}

					[[nodiscard]] auto value() const -> string_view_type
					{
						return get<2>();
					}
				};

				using result_type = InsertResult;

			private:
				group_type&	  group_;
				read_accessor read_accessor_;

				explicit GroupAccessor(group_type& group, const string_view_type name)
					: group_{group},
					  read_accessor_{group_, name} {}

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return read_accessor_.name();
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return read_accessor_.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return read_accessor_.size();
				}

				[[nodiscard]] auto contains(const string_view_type key) const -> bool
				{
					return read_accessor_.contains(key);
				}

				[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
				{
					return read_accessor_.get(key);
				}

				auto try_insert(const string_type& key, string_type&& value) -> result_type
				{
					return result_type{group_.try_emplace(key, std::move(value))};
				}

				auto try_insert(string_type&& key, string_type&& value) -> result_type
				{
					return result_type{group_.try_emplace(std::move(key), std::move(value))};
				}

				auto try_insert(node_type&& node) -> result_type
				{
					const auto [it, inserted, inserted_node] = group_.insert(std::move(node));
					return result_type{{it, inserted}};
				}

				auto insert_or_assign(const string_type& key, string_type&& value) -> result_type
				{
					return result_type{group_.insert_or_assign(key, std::move(value))};
				}

				auto insert_or_assign(string_type&& key, string_type&& value) -> result_type
				{
					return result_type{group_.insert_or_assign(std::move(key), std::move(value))};
				}

				auto insert_or_assign(node_type&& node) -> result_type;

				auto remove(const string_type& key) -> bool
				{
					return group_.erase(key);
				}

				auto remove(const string_view_type key) -> bool
				{
					// todo: cxx23 required
					// return group_.erase(key);
					return remove(string_type{key});
				}

				auto extract(const string_type& key) -> node_type
				{
					return node_type{group_.extract(key)};
				}

				auto extract(const string_view_type key) -> node_type
				{
					// todo: cxx23 required
					// return node_type{group_.extract(key)};
					return extract(string_type{key});
				}
			};

			template<>
			class GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>
			{
				friend IniReaderWithComment;
				friend GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;

			public:
				struct variable_with_comment
				{
					comment_type comment;
					string_type	 variable;
					comment_type inline_comment;
				};

				// comment
				// key = value inline_comment
				using group_type = std::unordered_map<string_type, variable_with_comment, detail::hash_type, std::equal_to<>>;

				// comment
				// [group_name] inline_comment
				// variables...
				struct self_type
				{
					group_type	 group;
					comment_type comment;
					comment_type inline_comment;
				};

			private:
				const self_type& self_;
				string_view_type name_;

				explicit GroupAccessor(const self_type& self, const string_view_type name)
					: self_{self},
					  name_{name} {}

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return name_;
				}

				[[nodiscard]] auto has_comment() const noexcept -> bool
				{
					return !self_.comment.empty();
				}

				[[nodiscard]] auto has_inline_comment() const noexcept -> bool
				{
					return !self_.inline_comment.empty();
				}

				[[nodiscard]] auto comment() const noexcept -> comment_view_type
				{
					return self_.comment;
				}

				[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type
				{
					return self_.inline_comment;
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return self_.group.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return self_.group.size();
				}

				[[nodiscard]] auto contains(const string_view_type key) const -> bool
				{
					return self_.group.contains(key);
				}

			private:
				[[nodiscard]] static auto get_it(group_type& group, const string_view_type key) -> group_type::iterator
				{
					return
					// todo
#ifdef GAL_INI_COMPILER_GNU
							group_.find(string_type{key});
#else
							group.find(key);
#endif
				}

				[[nodiscard]] static auto get_it(const group_type& group, const string_view_type key) -> group_type::const_iterator
				{
					return get_it(const_cast<group_type&>(group), key);
				}

			public:
				[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
				{
					if (const auto it = get_it(self_.group, key);
						it != self_.group.end())
					{
						return it->second.variable;
					}
					return {};
				}

				[[nodiscard]] auto has_comment(const string_view_type key) const -> bool
				{
					if (const auto it = get_it(self_.group, key);
						it == self_.group.end())
					{
						return false;
					}
					else
					{
						return !it->second.comment.empty();
					}
				}

				[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool
				{
					if (const auto it = get_it(self_.group, key);
						it == self_.group.end())
					{
						return false;
					}
					else
					{
						return !it->second.inline_comment.empty();
					}
				}

				[[nodiscard]] auto get_comment(const string_view_type key) const -> comment_view_type
				{
					if (const auto it = get_it(self_.group, key);
						it != self_.group.end())
					{
						return it->second.comment;
					}
					return {};
				}

				[[nodiscard]] auto get_inline_comment(const string_view_type key) const -> comment_view_type
				{
					if (const auto it = get_it(self_.group, key);
						it != self_.group.end())
					{
						return it->second.inline_comment;
					}
					return {};
				}
			};

			template<>
			class GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>
			{
				friend IniReaderWithComment;

			public:
				using read_accessor = GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>;

				using group_type	= read_accessor::group_type;
				using self_type		= read_accessor::self_type;

				class Node
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;

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

					explicit Node(group_type::node_type&& node) : node_{std::move(node)} {}

					explicit(false) operator group_type::node_type&&() &&
					{
						return std::move(node_);
					}

				public:
					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() const& -> const index_type<Index>&
					{
						if constexpr (Index == 0)
						{
							return node_.mapped().comment;
						}
						else if constexpr (Index == 1)
						{
							return node_.key();
						}
						else if constexpr (Index == 2)
						{
							return node_.mapped().variable;
						}
						else if constexpr (Index == 3)
						{
							return node_.mapped().inline_comment;
						}
					}

					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() && -> index_type<Index>&&
					{
						if constexpr (Index == 0)
						{
							return std::move(node_.mapped().comment);
						}
						else if constexpr (Index == 1)
						{
							return std::move(node_.key());
						}
						else if constexpr (Index == 2)
						{
							return std::move(node_.mapped().variable);
						}
						else if constexpr (Index == 3)
						{
							return std::move(node_.mapped().inline_comment);
						}
					}

					[[nodiscard]] auto comment() const& -> comment_view_type
					{
						return get<0>();
					}

					[[nodiscard]] auto comment() && -> comment_type&&
					{
						return std::move(*this).get<0>();
					}

					[[nodiscard]] auto key() const& -> string_view_type
					{
						return get<1>();
					}

					[[nodiscard]] auto key() && -> string_type&&
					{
						return std::move(*this).get<1>();
					}

					[[nodiscard]] auto value() const& -> string_view_type
					{
						return get<2>();
					}

					[[nodiscard]] auto value() && -> string_type&&
					{
						return std::move(*this).get<2>();
					}

					[[nodiscard]] auto inline_comment() const& -> comment_view_type
					{
						return get<3>();
					}

					[[nodiscard]] auto inline_comment() && -> comment_type&&
					{
						return std::move(*this).get<3>();
					}

					[[nodiscard]] explicit operator bool() const noexcept
					{
						return node_.operator bool();
					}

					[[nodiscard]] auto empty() const noexcept -> bool
					{
						return node_.empty();
					}
				};

				using node_type = Node;

				class InsertResult
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;

				public:
					constexpr static std::size_t max_elements_size = node_type::max_elements_size + 1;

					template<std::size_t Index>
						requires(Index < max_elements_size)
					using index_type = std::conditional<
							Index == 0,
							bool,
							node_type::index_type<Index - 1>>::type;

				private:
					using result_type = std::pair<group_type::const_iterator, bool>;

					result_type result_;

					explicit InsertResult(result_type&& result) : result_{std::move(result)} {}

				public:
					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() const -> const index_type<Index>&
					{
						if constexpr (Index == 0)
						{
							return result_.second;
						}
						else if constexpr (Index == 1)
						{
							return result_.first->second.comment;
						}
						else if constexpr (Index == 2)
						{
							return result_.first->first;
						}
						else if constexpr (Index == 3)
						{
							return result_.first->second.variable;
						}
						else if constexpr (Index == 4)
						{
							return result_.first->second.inline_comment;
						}
					}

					[[nodiscard]] explicit operator bool() const noexcept
					{
						return result_.second;
					}

					[[nodiscard]] auto result() const -> bool
					{
						return get<0>();
					}

					[[nodiscard]] auto comment() const -> comment_view_type
					{
						return get<1>();
					}

					[[nodiscard]] auto key() const -> string_view_type
					{
						return get<2>();
					}

					[[nodiscard]] auto value() const -> string_view_type
					{
						return get<3>();
					}

					[[nodiscard]] auto inline_comment() const -> comment_view_type
					{
						return get<4>();
					}
				};

				using result_type = InsertResult;

			private:
				self_type&	  self_;
				read_accessor read_accessor_;

				explicit GroupAccessor(self_type& self, const string_view_type name)
					: self_{self},
					  read_accessor_{self_, name} {}

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return read_accessor_.name();
				}

				[[nodiscard]] auto has_comment() const noexcept -> bool
				{
					return read_accessor_.has_comment();
				}

				[[nodiscard]] auto has_inline_comment() const noexcept -> bool
				{
					return read_accessor_.has_inline_comment();
				}

				[[nodiscard]] auto comment() const noexcept -> comment_view_type
				{
					return read_accessor_.comment();
				}

				[[nodiscard]] auto inline_comment() const noexcept -> comment_view_type
				{
					return read_accessor_.inline_comment();
				}

				auto comment(comment_type&& comment) -> void
				{
					self_.comment = std::move(comment);
				}

				auto inline_comment(comment_type&& inline_comment) -> void
				{
					self_.inline_comment = std::move(inline_comment);
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return read_accessor_.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return read_accessor_.size();
				}

				[[nodiscard]] auto contains(const string_view_type key) const -> bool
				{
					return read_accessor_.contains(key);
				}

				[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
				{
					return read_accessor_.get(key);
				}

				[[nodiscard]] auto has_comment(const string_view_type key) const -> bool
				{
					return read_accessor_.has_comment(key);
				}

				[[nodiscard]] auto has_inline_comment(const string_view_type key) const -> bool
				{
					return read_accessor_.has_inline_comment(key);
				}

				[[nodiscard]] auto get_comment(const string_view_type key) const -> comment_view_type
				{
					return read_accessor_.get_comment(key);
				}

				[[nodiscard]] auto get_inline_comment(const string_view_type key) const -> comment_view_type
				{
					return read_accessor_.get_inline_comment(key);
				}

				auto try_insert(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type
				{
					return result_type{self_.group.try_emplace(key, read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})};
				}

				auto try_insert(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type
				{
					return result_type{self_.group.try_emplace(std::move(key), read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})};
				}

				auto try_insert(node_type&& node) -> result_type
				{
					const auto [it, inserted, inserted_node] = self_.group.insert(std::move(node));
					return result_type{{it, inserted}};
				}

				auto insert_or_assign(const string_type& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type
				{
					return result_type{self_.group.insert_or_assign(key, read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})};
				}

				auto insert_or_assign(string_type&& key, string_type&& value, comment_type&& comment = {}, comment_type&& inline_comment = {}) -> result_type
				{
					return result_type{self_.group.insert_or_assign(std::move(key), read_accessor::variable_with_comment{std::move(comment), std::move(value), std::move(inline_comment)})};
				}

				auto insert_or_assign(node_type&& node) -> result_type;

				auto remove(const string_type& key) -> bool
				{
					return self_.group.erase(key);
				}

				auto remove(const string_view_type key) -> bool
				{
					// todo: cxx23 required
					// return group_.erase(key);
					return remove(string_type{key});
				}

				auto extract(const string_type& key) -> node_type
				{
					return node_type{self_.group.extract(key)};
				}

				auto extract(const string_view_type key) -> node_type
				{
					// todo: cxx23 required
					// return node_type{group_.extract(key)};
					return extract(string_type{key});
				}
			};

			template<>
			class GroupAccessor<GroupProperty::READ_ORDERED>
			{
				friend IniOrderedReader;
				friend GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>;

			public:
				// signed type, this allows us to simply specify that the line is smaller than the key when inserting a value in front of a key.
				using line_type	 = int;
				// line <=> key = value
				using group_type = std::multimap<line_type, std::pair<string_type, string_type>>;

			private:
				const group_type& group_;
				string_view_type  name_;

				explicit GroupAccessor(const group_type& group, const string_view_type name)
					: group_{group},
					  name_{name} {}

				[[nodiscard]] static auto get_it(group_type& group, string_view_type key) -> group_type::iterator;
				[[nodiscard]] static auto get_it(const group_type& group, string_view_type key) -> group_type::const_iterator;

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return name_;
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return group_.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return group_.size();
				}

				[[nodiscard]] auto contains(string_view_type key) const -> bool;

				[[nodiscard]] auto get(string_view_type key) const -> string_view_type;
			};

			template<>
			class GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>
			{
				using read_accessor_type = GroupAccessor<GroupProperty::READ_ORDERED>;

			public:
				using line_type	 = read_accessor_type::line_type;
				using group_type = read_accessor_type::group_type;

				class Node
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY_ORDERED>;

				public:
					constexpr static std::size_t max_elements_size = 2;

					template<std::size_t Index>
						requires(Index < max_elements_size)
					using index_type = std::conditional_t<
							Index == 0,
							string_type,
							std::conditional_t<
									Index == 1,
									string_type,
									void>>;

				private:
					group_type::node_type node_;

					explicit Node(group_type::node_type&& node) : node_{std::move(node)} {}

					explicit(false) operator group_type::node_type&&() &&
					{
						return std::move(node_);
					}

				public:
					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() const& -> const string_type&
					{
						if constexpr (Index == 0)
						{
							return node_.mapped().first;
						}
						else if constexpr (Index == 1)
						{
							return node_.mapped().second;
						}
					}

					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() && -> string_type&&
					{
						if constexpr (Index == 0)
						{
							return std::move(node_.mapped().first);
						}
						else if constexpr (Index == 1)
						{
							return std::move(node_.mapped().second);
						}
					}

					[[nodiscard]] auto key() const& -> string_view_type
					{
						return get<0>();
					}

					[[nodiscard]] auto key() && -> string_type&&
					{
						return std::move(*this).get<0>();
					}

					[[nodiscard]] auto value() const& -> string_view_type
					{
						return get<1>();
					}

					[[nodiscard]] auto value() && -> string_type&&
					{
						return std::move(*this).get<1>();
					}

					[[nodiscard]] explicit operator bool() const noexcept
					{
						return node_.operator bool();
					}

					[[nodiscard]] auto empty() const noexcept -> bool
					{
						return node_.empty();
					}
				};

				using node_type = Node;

			private:
				group_type&		   group_;
				read_accessor_type read_accessor_;

				explicit GroupAccessor(group_type& group, const string_view_type name)
					: group_{group},
					  read_accessor_{group_, name} {}

			public:
				[[nodiscard]] auto name() const noexcept -> string_view_type
				{
					return read_accessor_.name();
				}

				[[nodiscard]] auto empty() const noexcept -> bool
				{
					return read_accessor_.empty();
				}

				[[nodiscard]] auto size() const noexcept -> group_type::size_type
				{
					return read_accessor_.size();
				}

				[[nodiscard]] auto contains(const string_view_type key) const -> bool
				{
					return read_accessor_.contains(key);
				}

				[[nodiscard]] auto get(const string_view_type key) const -> string_view_type
				{
					return read_accessor_.get(key);
				}

				auto try_insert(const string_type& key, string_type&& value) -> bool;

				auto try_insert(string_type&& key, string_type&& value) -> bool;

				auto try_insert(node_type&& node) -> bool;

				auto insert_or_assign(const string_type& key, string_type&& value) -> bool;

				auto insert_or_assign(string_type&& key, string_type&& value) -> bool;

				auto insert_or_assign(node_type&& node) -> bool;

			private:
				auto get_it(string_view_type target_key, string_view_type key) -> std::pair<group_type::iterator, group_type::iterator>;

			public:
				auto try_insert_before(string_view_type target_key, string_view_type key, string_type&& value) -> bool;

				auto try_insert_before(string_view_type target_key, string_type&& key, string_type&& value) -> bool;

				auto try_insert_after(string_view_type target_key, string_view_type key, string_type&& value) -> bool;

				auto try_insert_after(string_view_type target_key, string_type&& key, string_type&& value) -> bool;

				auto insert_or_assign_before(string_view_type target_key, string_view_type key, string_type&& value) -> bool;

				auto insert_or_assign_before(string_view_type target_key, string_type&& key, string_type&& value) -> bool;

				auto insert_or_assign_after(string_view_type target_key, string_view_type key, string_type&& value) -> bool;

				auto insert_or_assign_after(string_view_type target_key, string_type&& key, string_type&& value) -> bool;

				auto remove(const string_type& key) -> bool
				{
					if (const auto it = read_accessor_type::get_it(group_, key);
						it != group_.end())
					{
						group_.erase(it);
						return true;
					}
					return false;
				}

				auto remove(const string_view_type key) -> bool
				{
					if (const auto it = read_accessor_type::get_it(group_, key);
						it != group_.end())
					{
						group_.erase(it);
						return true;
					}
					return false;
				}

				auto extract(const string_type& key) -> node_type
				{
					if (const auto it = read_accessor_type::get_it(group_, key);
						it != group_.end())
					{
						return node_type{group_.extract(it)};
					}
					return node_type{{}};
				}

				auto extract(const string_view_type key) -> node_type
				{
					if (const auto it = read_accessor_type::get_it(group_, key);
						it != group_.end())
					{
						return node_type{group_.extract(it)};
					}
					return node_type{{}};
				}
			};
		}// namespace detail

		// Read the data from the ini file and cannot be modified or written back to the file. This means that we do not need to care about the order of the data.
		class IniReader
		{
		public:
			using reader_type  = detail::GroupAccessor<GroupProperty::READ_ONLY>;
			using writer_type  = detail::GroupAccessor<GroupProperty::READ_MODIFY>;

			// key = value
			using group_type   = reader_type::group_type;
			// [group_name]
			// key1 = value1
			// key2 = value2
			// key3 = value3
			// ...
			using context_type = std::unordered_map<string_type, group_type, detail::hash_type, std::equal_to<>>;

		private:
			context_type context_;

		public:
			// It is no need to write data back to the file, which means that we no longer need the filename, just read the data when construct reader.
			explicit IniReader(filename_view_type filename);

			[[nodiscard]] auto empty() const noexcept -> bool
			{
				return context_.empty();
			}

			[[nodiscard]] auto size() const noexcept -> context_type::size_type
			{
				return context_.size();
			}

			[[nodiscard]] auto contains(const string_view_type group_name) const -> bool
			{
				return context_.contains(group_name);
			}

			[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
			{
				if (const auto it =
#ifdef GAL_INI_COMPILER_GNU
							context_.find(string_type{group_name});
#else
							context_.find(group_name);
#endif
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

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
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

				const auto result = context_.emplace(group_name, group_type{}).first;
				return reader_type{result->second, result->first};
			}

			[[nodiscard]] auto read(string_type&& group_name) -> reader_type
			{
				if (const auto it = context_.find(group_name);
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

				const auto result = context_.emplace(std::move(group_name), group_type{}).first;
				return reader_type{result->second, result->first};
			}

			[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
			{
				if (const auto it =
#ifdef GAL_INI_COMPILER_GNU
							context_.find(string_type{group_name});
#else
							context_.find(group_name);
#endif
					it != context_.end())
				{
					return writer_type{it->second, it->first};
				}

				auto result = context_.emplace(group_name, group_type{}).first;
				return writer_type{result->second, result->first};
			}

			[[nodiscard]] auto write(string_type&& group_name) -> writer_type
			{
				if (const auto it = context_.find(group_name);
					it != context_.end())
				{
					return writer_type{it->second, it->first};
				}

				auto result = context_.emplace(std::move(group_name), group_type{}).first;
				return writer_type{result->second, result->first};
			}

			template<typename OStream>
			auto print(OStream&				  out,
					   const string_view_type separator = detail::line_separator<string_view_type::value_type>()) const
				requires requires {
							 out << separator.data();
							 out << string_type{};
						 }
			{
				for (const auto& [group_name, variables]: context_)
				{
					out << "[" << group_name << "]" << separator.data();
					for (const auto& [variable_key, variable_value]: variables)
					{
						out << variable_key << "=" << variable_value << separator.data();
					}
				}
			}
		};

		// Read the data from the ini file and cannot be modified or written back to the file. This means that we do not need to care about the order of the data.
		class IniReaderWithComment
		{
		public:
			using reader_type  = detail::GroupAccessor<GroupProperty::READ_ONLY_WITH_COMMENT>;
			using writer_type  = detail::GroupAccessor<GroupProperty::READ_MODIFY_WITH_COMMENT>;

			// comment
			// key = value inline_comment
			using group_type   = reader_type::self_type;
			// comment
			// [group_name] inline_comment
			// key1 = value1
			// key2 = value2
			// key3 = value3
			// ...
			using context_type = std::unordered_map<string_type, group_type, detail::hash_type, std::equal_to<>>;

		private:
			context_type context_;

		public:
			// It is no need to write data back to the file, which means that we no longer need the filename, just read the data when construct reader.
			explicit IniReaderWithComment(filename_view_type filename);

			[[nodiscard]] auto empty() const noexcept -> bool
			{
				return context_.empty();
			}

			[[nodiscard]] auto size() const noexcept -> context_type::size_type
			{
				return context_.size();
			}

			[[nodiscard]] auto contains(const string_view_type group_name) const -> bool
			{
				return context_.contains(group_name);
			}

			[[nodiscard]] auto read(const string_view_type group_name) const -> reader_type
			{
				if (const auto it =
#ifdef GAL_INI_COMPILER_GNU
							context_.find(string_type{group_name});
#else
							context_.find(group_name);
#endif
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

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
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

				const auto result = context_.emplace(group_name, group_type{}).first;
				return reader_type{result->second, result->first};
			}

			[[nodiscard]] auto read(string_type&& group_name) -> reader_type
			{
				if (const auto it = context_.find(group_name);
					it != context_.end())
				{
					return reader_type{it->second, it->first};
				}

				const auto result = context_.emplace(std::move(group_name), group_type{}).first;
				return reader_type{result->second, result->first};
			}

			[[nodiscard]] auto write(const string_view_type group_name) -> writer_type
			{
				if (const auto it =
#ifdef GAL_INI_COMPILER_GNU
							context_.find(string_type{group_name});
#else
							context_.find(group_name);
#endif
					it != context_.end())
				{
					return writer_type{it->second, it->first};
				}

				auto result = context_.emplace(group_name, group_type{}).first;
				return writer_type{result->second, result->first};
			}

			[[nodiscard]] auto write(string_type&& group_name) -> writer_type
			{
				if (const auto it = context_.find(group_name);
					it != context_.end())
				{
					return writer_type{it->second, it->first};
				}

				auto result = context_.emplace(std::move(group_name), group_type{}).first;
				return writer_type{result->second, result->first};
			}

			template<typename OStream>
			auto print(OStream&				  out,
					   const string_view_type separator = detail::line_separator<string_view_type::value_type>()) const
				requires requires {
							 out << separator.data();
							 out << string_type{};
						 }
			{
				for (const auto& [group_name, group_self]: context_)
				{
					if (!group_self.comment.empty())
					{
						out << group_self.comment.indication << ' ' << group_self.comment.comment << separator.data();
					}

					out << "[" << group_name << "]";
					if (!group_self.inline_comment.empty())
					{
						out << ' ' << group_self.inline_comment.indication << ' ' << group_self.inline_comment.comment;
					}
					out << separator.data();

					for (const auto& [variable_key, variable_with_comment]: group_self.group)
					{
						if (!variable_with_comment.comment.empty())
						{
							out << variable_with_comment.comment.indication << ' ' << variable_with_comment.comment.comment << separator.data();
						}

						out << variable_key << "=" << variable_with_comment.variable;
						if (!variable_with_comment.inline_comment.empty())
						{
							out << ' ' << variable_with_comment.inline_comment.indication << ' ' << variable_with_comment.inline_comment.comment;
						}
						out << separator.data();
					}
				}
			}
		};
	}// namespace impl
}// namespace gal::ini

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::result_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type::max_elements_size;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::result_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::result_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::node_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_WITH_COMMENT>::result_type::index_type<Index>;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type>
{
	using type = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type::index_type<Index>;
};
