#pragma once

#include <memory>
#include <type_traits>

#if defined(GAL_INI_COMPILER_MSVC)
	#define GAL_INI_UNREACHABLE() __assume(0)
#elif defined(GAL_INI_COMPILER_GNU) || defined(GAL_INI_COMPILER_CLANG) || defined(GAL_INI_COMPILER_APPLE_CLANG)
	#define GAL_INI_UNREACHABLE() __builtin_unreachable()
#else
	#define GAL_INI_UNREACHABLE() throw
#endif

namespace gal::ini
{
	namespace group_accessor_detail
	{
		template<typename T>
		struct char_type;

		template<typename String>
			requires requires { typename String::value_type; } || requires { std::declval<String>()[0]; }
		struct char_type<String>
		{
			using type = std::conditional_t<
					requires { typename String::value_type; },
					typename String::value_type,
					std::decay_t<decltype(std::declval<String>()[0])>>;

			static_assert(std::is_integral_v<type>);
		};

		template<typename T>
		using char_type_t = char_type<T>::type;

		template<typename T>
		struct char_traits;

		template<typename String>
			requires requires { typename String::traits_type; } || requires { std::declval<String>()[0]; }
		struct char_traits<String>
		{
			using type = std::conditional_t<
					requires { typename String::value_type; },
					typename String::traits_type,
					std::char_traits<char_type_t<String>>>;
		};

		template<typename T>
		using char_traits_t = char_traits<T>::type;

		template<typename T>
		struct allocator_type;

		template<typename String>
			requires requires { typename String::allocator_type; } || requires { std::allocator<char_type_t<String>>{}; }
		struct allocator_type<String>
		{
			using type = std::conditional_t<
					requires { typename String::allocator_type; },
					typename String::allocator_type,
					std::allocator<char_type_t<String>>>;
		};

		template<typename T>
		class StackFunction;

		template<typename Return, typename... Args>
		class StackFunction<Return(Args...)>
		{
			using result_type  = Return;

			using invoker_type = auto (*)(const char*, Args&&...) -> result_type;

		private:
			invoker_type invoker_;
			const char*	 data_;

			template<typename Functor>
			[[nodiscard]] constexpr static auto do_invoke(Functor* functor, Args&&... args) noexcept(noexcept((*functor)(std::forward<Args>(args)...)))
					-> result_type
			{
				return (*functor)(std::forward<Args>(args)...);
			}

		public:
			constexpr StackFunction() noexcept
				: invoker_{nullptr},
				  data_{nullptr} {}

			template<typename Functor>
			constexpr explicit(false) StackFunction(const Functor& functor) noexcept
				: invoker_{reinterpret_cast<invoker_type>(do_invoke<Functor>)},
				  data_{reinterpret_cast<const char*>(&functor)}
			{
			}

			constexpr auto operator()(Args&&... args) noexcept(noexcept(invoker_(data_, std::forward<Args>(args)...))) -> result_type
			{
				return invoker_(data_, std::forward<Args>(args)...);
			}
		};
	}// namespace group_accessor_detail

	template<typename String>
	using string_view_t = std::basic_string_view<group_accessor_detail::char_type_t<String>, group_accessor_detail::char_traits_t<String>>;

	namespace group_accessor_detail
	{
		template<typename Table, typename String>
		struct table_finder
		{
			using table_type	   = Table;
			using string_type	   = String;
			using string_view_type = string_view_t<string_type>;

			// cxx 20
			template<typename T, typename S>
			struct transparent : std::false_type
			{
			};
			template<typename T, typename S>
				requires requires {
							 {
								 std::declval<T&>().find(std::declval<const string_view_t<S>>())
							 } -> std::same_as<typename T::iterator>;

							 {
								 std::declval<const T&>().find(std::declval<const string_view_t<S>>())
							 } -> std::same_as<typename T::const_iterator>;
						 }
			struct transparent<T, S> : std::true_type
			{
			};
			template<typename T, typename S>
			constexpr static bool		 transparent_v = transparent<T, S>::value;

			[[nodiscard]] constexpr auto operator()(table_type& table, const string_type& key) -> typename table_type::iterator
			{
				return table.find(key);
			}

			[[nodiscard]] constexpr auto operator()(const table_type& table, const string_type& key) -> typename table_type::const_iterator
			{
				return table.find(key);
			}

			[[nodiscard]] constexpr auto operator()(table_type& table, const string_view_type key) -> typename table_type::iterator
			{
				if constexpr (transparent_v<table_type, string_type>)
				{
					return table.find(key);
				}
				else
				{
					return this->operator()(table, string_type{key});
				}
			}

			[[nodiscard]] constexpr auto operator()(const table_type& table, const string_view_type key) -> typename table_type::const_iterator
			{
				if constexpr (transparent_v<table_type, string_type>)
				{
					return table.find(key);
				}
				else
				{
					return this->operator()(table, string_type{key});
				}
			}
		};

		template<typename Table, typename String>
		struct table_modifier
		{
			using table_type	   = Table;
			using string_type	   = String;
			using string_view_type = string_view_t<string_type>;

			// cxx 23
			template<typename T, typename S>
			struct transparent : std::false_type
			{
			};
			template<typename T, typename S>
				requires requires {
							 std::declval<T&>().erase(std::declval<const string_view_t<S>>());

							 {
								 std::declval<T&>().extract(std::declval<const string_view_t<S>>())
							 } -> std::same_as<typename T::node_type>;
						 }
			struct transparent<T, S> : std::true_type
			{
			};
			template<typename T, typename S>
			constexpr static bool				transparent_v = transparent<T, S>::value;

			[[nodiscard]] constexpr static auto erase(table_type& table, const string_type& key) -> decltype(auto)
			{
				return table.erase(key);
			}

			[[nodiscard]] constexpr static auto erase(table_type& table, const string_view_type key) -> decltype(auto)
			{
				if constexpr (transparent_v<table_type, string_type>)
				{
					return table.erase(key);
				}
				else
				{
					return erase(table, string_type{key});
				}
			}

			[[nodiscard]] constexpr static auto extract(table_type& table, const string_type& key) -> typename table_type::node_type
			{
				return table.extract(key);
			}

			[[nodiscard]] constexpr static auto extract(table_type& table, const string_view_type key) -> typename table_type::node_type
			{
				if constexpr (transparent_v<table_type, string_type>)
				{
					return table.extract(key);
				}
				else
				{
					return extract(table, string_type{key});
				}
			}
		};
	}// namespace group_accessor_detail

	template<typename GroupType>
	class Reader;
	template<typename GroupType>
	class Writer;
	template<typename GroupType>
	class Flusher;

	template<typename GroupType>
	class Reader
	{
	public:
		using group_type		= GroupType;

		using key_type			= typename group_type::key_type;
		using key_view_type		= string_view_t<key_type>;
		using mapped_type		= typename group_type::mapped_type;
		using mapped_view_type	= string_view_t<mapped_type>;

		using name_type			= string_view_t<key_type>;

		using table_finder_type = group_accessor_detail::table_finder<group_type, key_type>;

	private:
		name_type		  name_;
		const group_type& group_;

	public:
		constexpr Reader(const name_type name, const group_type& group) noexcept
			: name_{name},
			  group_{group} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] constexpr auto name() const noexcept -> name_type { return name_; }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] constexpr auto empty() const noexcept -> bool { return group_.empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] constexpr auto size() const noexcept -> group_type::size_type { return group_.size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] constexpr auto contains(const key_view_type key) const -> bool { return group_.contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] constexpr auto get(const key_view_type key) const -> mapped_view_type
		{
			if (const auto it = table_finder_type{}(group_, key);
				it != group_.end())
			{
				return it->second;
			}

			return {};
		}
	};

	template<typename GroupType>
	class Writer
	{
		using reader_type = Reader<GroupType>;

	public:
		using group_type		  = reader_type::group_type;

		using key_type			  = reader_type::key_type;
		using key_view_type		  = reader_type::key_view_type;
		using mapped_type		  = reader_type::mapped_type;
		using mapped_view_type	  = reader_type::mapped_view_type;

		using name_type			  = reader_type::name_type;

		using table_finder_type	  = reader_type::table_finder_type;
		using table_modifier_type = group_accessor_detail::table_modifier<group_type, key_type>;

		struct node_type
		{
			friend Writer;

			using writer_node_type_hack_alias			   = group_type;

			constexpr static std::size_t max_elements_size = 2;

			template<std::size_t Index>
			// requires (Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					key_type,
					std::conditional_t<
							Index == 1,
							mapped_type,
							void>>;

		private:
			group_type::node_type node_;

			constexpr explicit node_type(group_type::node_type&& node)
				: node_{std::move(node)} {}

			constexpr explicit(false) operator typename group_type::node_type &&() && { return std::move(node_); }

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] constexpr auto get() const& -> const index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.key(); }
				else if constexpr (Index == 1) { return node_.mapped(); }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] constexpr auto get() & -> index_type<Index>&
			{
				if constexpr (Index == 0) { return node_.key(); }
				else if constexpr (Index == 1) { return node_.mapped(); }
				else { GAL_INI_UNREACHABLE(); }
			}

			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] constexpr auto get() && -> index_type<Index>&&
			{
				if constexpr (Index == 0) { return std::move(node_.key()); }
				else if constexpr (Index == 1) { return std::move(node_.mapped()); }
				else { GAL_INI_UNREACHABLE(); }
			}

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] constexpr auto	 key() const& -> key_view_type { return get<0>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] constexpr auto	 key() & -> key_view_type { return get<0>(); }

			/**
			 * @brief Get the node's key.
			 * @return The key of the node.
			 */
			[[nodiscard]] constexpr auto	 key() && -> key_type&& { return std::move(*this).template get<0>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] constexpr auto	 value() const& -> mapped_view_type { return get<1>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] constexpr auto	 value() & -> mapped_view_type { return get<1>(); }

			/**
			 * @brief Get the node's value.
			 * @return The value of the node.
			 */
			[[nodiscard]] constexpr auto	 value() && -> mapped_type&& { return std::move(*this).template get<1>(); }

			/**
			 * @brief Determine if the node is valid (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] constexpr explicit operator bool() const noexcept { return node_.operator bool(); }

			/**
			 * @brief Determine if the node is valid. (i.e. originally existed in the group)
			 * @return The node is valid or not.
			 */
			[[nodiscard]] constexpr auto	 empty() const noexcept -> bool { return node_.empty(); }
		};

		struct result_type
		{
			friend Writer;

			using writer_result_type_hack_alias			   = group_type;

			constexpr static std::size_t max_elements_size = node_type::max_elements_size + 1;

			template<std::size_t Index>
				requires(Index < max_elements_size)
			using index_type = std::conditional_t<
					Index == 0,
					bool,
					typename node_type::template index_type<Index - 1>>;

		private:
			using type = std::pair<typename group_type::const_iterator, bool>;

			type result_;

			constexpr explicit result_type(type result)
				: result_{result} {}

		public:
			template<std::size_t Index>
				requires(Index < max_elements_size)
			[[nodiscard]] constexpr auto get() const -> const index_type<Index>&
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
			[[nodiscard]] constexpr explicit operator bool() const noexcept { return get<0>(); }

			/**
			 * @brief Determine if the insertion was successful.
			 * @return The insertion was successes or not.
			 * @note If it overwrites an existing value, it is considered not inserted.
			 */
			[[nodiscard]] constexpr auto	 result() const -> bool { return get<0>(); }

			/**
			 * @brief Get the inserted key.
			 * @return The key of the insertion.
			 */
			[[nodiscard]] constexpr auto	 key() const -> key_view_type { return get<1>(); }

			/**
			 * @brief Get the inserted value.
			 * @return The value of the insertion.
			 */
			[[nodiscard]] constexpr auto	 value() const -> mapped_view_type { return get<2>(); }
		};

	private:
		name_type					 name_;
		group_type&					 group_;

		[[nodiscard]] constexpr auto as_reader() const noexcept -> const reader_type&
		{
			// :)
			return reinterpret_cast<const reader_type&>(*this);
		}

		[[nodiscard]] constexpr auto propagate_rep() const noexcept -> const group_type& { return group_; }

		[[nodiscard]] constexpr auto propagate_rep() noexcept -> group_type& { return group_; }

	public:
		constexpr Writer(const name_type name, group_type& group) noexcept
			: name_{name},
			  group_{group} {}

		/**
		 * @brief Get the name of the group.
		 * @return The name of the group.
		 */
		[[nodiscard]] constexpr auto name() const noexcept -> name_type { return as_reader().name(); }

		/**
		 * @brief Get whether the group is empty.
		 * @return The group is empty or not.
		 */
		[[nodiscard]] constexpr auto empty() const noexcept -> bool { return as_reader().empty(); }

		/**
		 * @brief Get the number of values in the group.
		 * @return The number of values in the group.
		 */
		[[nodiscard]] constexpr auto size() const noexcept -> group_type::size_type { return as_reader().size(); }

		/**
		 * @brief Check whether the group contains the key.
		 * @param key The key to find.
		 * @return The group contains the key or not.
		 */
		[[nodiscard]] constexpr auto contains(const key_view_type key) const -> bool { return as_reader().contains(key); }

		/**
		 * @brief Get the value corresponding to the key in the group. If the key does not exist, return empty value.
		 * @param key The key to find.
		 * @return The value corresponding to the key.
		 */
		[[nodiscard]] constexpr auto get(const key_view_type key) const -> mapped_view_type { return as_reader().get(key); }

		/**
		 * @brief Insert a new key-value pair, or do nothing if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		constexpr auto				 try_insert(const key_type& key, mapped_type&& value) -> result_type
		{
			return result_type{propagate_rep().try_emplace(key, std::move(value))};
		}

		/**
		 * @brief Insert a new key-value pair, or do nothing if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		constexpr auto try_insert(key_type&& key, mapped_type&& value) -> result_type { return result_type{propagate_rep().try_emplace(std::move(key), std::move(value))}; }

		/**
		 * @brief Insert a node previously released from the group, or do nothing if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		constexpr auto try_insert(node_type&& node) -> result_type
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
		constexpr auto insert_or_assign(const key_type& key, mapped_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(key, std::move(value))}; }

		/**
		 * @brief Insert a new key-value pair, or assign if it already exists.
		 * @param key Key to insert.
		 * @param value Value to insert.
		 * @return The result of this insertion.
		 */
		constexpr auto insert_or_assign(key_type&& key, mapped_type&& value) -> result_type { return result_type{propagate_rep().insert_or_assign(std::move(key), std::move(value))}; }

		/**
		 * @brief Insert a node previously released from the group, or assign if it already exists.
		 * @param node Node to insert.
		 * @return The result of this insertion.
		 */
		constexpr auto insert_or_assign(node_type&& node) -> result_type;

		/**
		 * @brief Remove a key-value pair from a group.
		 * @param key The key of the pair.
		 * @return Whether the removal is successful or not. (If the key does not exist, the removal fails)
		 */
		constexpr auto remove(const key_view_type key) -> bool
		{
			return table_modifier_type::erase(propagate_rep(), key);
		}

		/**
		 * @brief Release a node from the group. (After that you can change the key/value of the node and insert it back into the group)
		 * @param key The key of the pair.
		 * @return The node.
		 */
		constexpr auto extract(const key_view_type key) -> node_type
		{
			return node_type{table_modifier_type::extract(propagate_rep(), key)};
		}
	};
}// namespace gal::ini

// ===========================
// WRITER
// ===========================

//template<typename GroupType>
//struct std::tuple_size<typename gal::ini::Writer<GroupType>::node_type>
template<typename T>
	requires std::same_as<T, typename gal::ini::Writer<typename T::writer_node_type_hack_alias>>
struct std::tuple_size<T>
{
	constexpr static std::size_t value = T::max_elements_size;
};

//template<typename GroupType>
//struct std::tuple_size<gal::ini::Writer<GroupType>::result_type>
template<typename T>
	requires std::same_as<T, typename gal::ini::Writer<typename T::writer_result_type_hack_alias>>
struct std::tuple_size<T>
{
	constexpr static std::size_t value = T::max_elements_size;
};

//template<std::size_t Index, typename GroupType>
//struct std::tuple_element<Index, typename gal::ini::Writer<GroupType>::node_type>
template<std::size_t Index, typename T>
	requires std::same_as<T, typename gal::ini::Writer<typename T::writer_node_type_hack_alias>>
struct std::tuple_element<Index, T>
{
	using type = T::template index_type<Index>;
};

//template<std::size_t Index, typename GroupType>
//struct std::tuple_element<Index, typename gal::ini::Writer<GroupType>::result_type>
template<std::size_t Index, typename T>
	requires std::same_as<T, typename gal::ini::Writer<typename T::writer_result_type_hack_alias>>
struct std::tuple_element<Index, T>
{
	using type = T::template index_type<Index>;
};

namespace gal::ini
{
	template<typename GroupType>
	constexpr auto Writer<GroupType>::insert_or_assign(node_type&& node) -> result_type
	{
		auto&& [key, value] = std::move(node);
		return insert_or_assign(std::move(key), std::move(value));
	}
}// namespace gal::ini
