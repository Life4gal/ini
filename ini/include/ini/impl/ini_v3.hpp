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

	enum class GroupProperty
	{
		READ_ONLY,
		READ_MODIFY,

		READ_ORDERED,
		READ_MODIFY_ORDERED,
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
		class IniOrderedReader;

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
				using group_type = GroupAccessor<GroupProperty::READ_ONLY>::group_type;

				class Node
				{
					friend GroupAccessor<GroupProperty::READ_MODIFY>;

				public:
					constexpr static std::size_t max_elements_size = 2;

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
							return node_.key();
						}
						else if constexpr (Index == 1)
						{
							return node_.mapped();
						}
					}

					template<std::size_t Index>
						requires(Index < max_elements_size)
					[[nodiscard]] auto get() && -> string_type&&
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

			private:
				group_type&								group_;
				GroupAccessor<GroupProperty::READ_ONLY> read_accessor_;

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

				auto try_insert(const string_type& key, string_type&& value) -> bool
				{
					const auto [it, inserted] = group_.try_emplace(key, std::move(value));
					return inserted;
				}

				auto try_insert(string_type&& key, string_type&& value) -> bool
				{
					const auto [it, inserted] = group_.try_emplace(std::move(key), std::move(value));
					return inserted;
				}

				auto try_insert(node_type&& node) -> bool
				{
					const auto [it, inserted, inserted_node] = group_.insert(std::move(node));
					return inserted;
				}

				auto insert_or_assign(const string_type& key, string_type&& value) -> bool
				{
					const auto [it, inserted] = group_.insert_or_assign(key, std::move(value));
					return inserted;
				}

				auto insert_or_assign(string_type&& key, string_type&& value) -> bool
				{
					const auto [it, inserted] = group_.insert_or_assign(std::move(key), std::move(value));
					return inserted;
				}

				auto insert_or_assign(node_type&& node) -> bool;

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
	}// namespace impl
}// namespace gal::ini

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type::max_elements_size;
};

template<>
struct ::std::tuple_size<gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type>
{
	constexpr static std::size_t value = gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type::max_elements_size;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY>::node_type>
{
	using type = gal::ini::string_type;
};

template<std::size_t Index>
struct ::std::tuple_element<Index, gal::ini::impl::detail::GroupAccessor<gal::ini::GroupProperty::READ_MODIFY_ORDERED>::node_type>
{
	using type = gal::ini::string_type;
};
