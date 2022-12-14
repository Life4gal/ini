#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#ifndef GAL_INI_FLAT_MAP_TYPE
// #include <vector>
	#include <map>
#endif

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

#ifndef GAL_INI_FLAT_MAP_TYPE
	// todo: Maybe we can distinguish between [need to detect duplicate keys] and [do not need to detect duplicate keys], which makes it easier.
	// The current use of [map] will cause the read [group] and [key] to be sorted based on string comparison, but this can ensure that the subsequent [read]/[write] order is consistent.
	template<typename Key, typename Value>
	using flat_map_type = std::map<Key, Value, std::less<>>;// std::vector<std::pair<Key, Value>>;
#else
	template<typename Key, typename Value>
	using flat_map_type = GAL_INI_FLAT_MAP_TYPE<Key, Value>;
#endif

	using group_type   = flat_map_type<string_type, string_type>;
	using context_type = flat_map_type<string_type, group_type>;

	namespace impl::inline v2
	{
		namespace detail
		{
			struct hash_type
			{
				using is_transparent = int;

				[[nodiscard]] auto operator()(const string_type &string) const noexcept -> std::size_t
				{
					return std::hash<string_type>{}(string);
				}

				[[nodiscard]] auto operator()(const string_view_type &string) const noexcept -> std::size_t
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
		}// namespace detail

		class Ini
		{
		public:
			constexpr static group_type::size_type no_group_specified{static_cast<group_type::size_type>(-1)};

		private:
			filename_type			filename_;
			context_type			context_;
			context_type ::iterator current_group_;

		public:
			explicit Ini(filename_type &&filename);

			explicit Ini(const filename_type &filename) : Ini{filename_type{filename}} {}

			~Ini() noexcept
			{
				flush();
			}

			Ini(const Ini &)					 = delete;


			auto operator=(const Ini &) -> Ini & = delete;
			Ini(Ini &&)							 = default;

			auto operator=(Ini &&) -> Ini	   & = default;

			auto begin_group(const string_view_type group_name) -> void
			{
				// todo
#ifdef GAL_INI_COMPILER_GNU
				current_group_ = context_.find(string_type{group_name});
#else
				current_group_ = context_.find(group_name);
#endif
			}

			template<typename ValueType>
			auto value(const string_type &key, ValueType &&value) -> void
				requires requires(group_type &group) {
							 group.insert_or_assign(key, std::forward<ValueType>(value));
						 }
			{
				if (current_group_ != context_.end())
				{
					auto &[_, group] = *current_group_;
					group.insert_or_assign(key, std::forward<ValueType>(value));
				}
			}

			template<typename ValueType>
			auto value(string_type &&key, ValueType &&value) -> void
				requires requires(group_type &group) {
							 group.insert_or_assign(std::move(key), std::forward<ValueType>(value));
						 }
			{
				if (current_group_ != context_.end())
				{
					auto &[_, group] = *current_group_;
					group.insert_or_assign(std::move(key), std::forward<ValueType>(value));
				}
			}

			[[nodiscard]] auto value(const string_view_type key) -> string_view_type
			{
				if (current_group_ != context_.end())
				{
					auto &[_, group] = *current_group_;
					if (const auto it =
					// todo
#ifdef GAL_INI_COMPILER_GNU
								group.find(string_type{key});
#else
								group.find(key);
#endif

						it != group.end())
					{
						return it->second;
					}
				}

				return {};
			}

			auto			   end_group() -> void { current_group_ = context_.end(); }

			[[nodiscard]] auto empty() const noexcept -> bool { return context_.empty(); }

			[[nodiscard]] auto groups() const noexcept -> context_type::size_type { return context_.size(); }

			[[nodiscard]] auto variables() const noexcept -> group_type::size_type
			{
				if (current_group_ != context_.end()) { return current_group_->second.size(); }
				return no_group_specified;
			}

			template<typename OStream>
			auto print(OStream				 &out,
					   const string_view_type separator = detail::line_separator<string_view_type::value_type>()) const
				requires requires {
							 out << separator.data();
							 out << string_type{};
						 }
			{
				for (const auto &[group_name, variables]: context_)
				{
					out << "[" << group_name << "]" << separator.data();
					for (const auto &[variable_key, variable_value]: variables)
					{
						out << variable_key << "=" << variable_value << separator.data();
					}
				}
			}

			auto flush() const -> void;
		};
	}// namespace impl::inline v2
}// namespace gal::ini
