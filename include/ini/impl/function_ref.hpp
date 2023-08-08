// Copyright (C) 2022-2023 Life4gal <life4gal@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#pragma once

// https://github.com/Life4gal/prometheus/blob/master/include/prometheus/infrastructure/utility.hpp

#include <type_traits>

namespace gal::prometheus::inline infrastructure
{
	namespace utility_detail
	{
		constexpr static auto compare_greater_than = [](const auto& lhs, const auto& rhs) noexcept(noexcept(lhs > rhs)) -> bool { return lhs > rhs; };
		constexpr static auto compare_less_than    = [](const auto& lhs, const auto& rhs) noexcept(noexcept(lhs < rhs)) -> bool { return lhs < rhs; };

		template<auto Functor>
		struct binary_invoker
		{
			template<typename L, typename R, typename... Ts>
			[[nodiscard]] constexpr auto operator()(
					const L&     lhs,
					const R&     rhs,
					const Ts&... reset)
			const noexcept(
				noexcept(Functor(lhs, rhs)) && noexcept((Functor(lhs, reset) && ...)) && noexcept((Functor(rhs, reset) && ...)))
				-> const auto&
			{
				if constexpr (sizeof...(reset) == 0) { return Functor(lhs, rhs) ? lhs : rhs; }
				else { return this->operator()(this->operator()(lhs, rhs), reset...); }
			}
		};

		using invoker_max = binary_invoker<compare_greater_than>;
		using invoker_min = binary_invoker<compare_less_than>;
	}// namespace utility_detail

	namespace functor
	{
		constexpr static utility_detail::invoker_max max;
		constexpr static utility_detail::invoker_min min;
	}// namespace functor
}    // namespace gal::prometheus::inline infrastructure

// https://github.com/Life4gal/prometheus/blob/master/include/prometheus/infrastructure/concepts.hpp

#include <type_traits>

namespace gal::prometheus::inline infrastructure::concepts
{
	template<typename T, typename... Ts>
	constexpr static bool any_of_v = (std::is_same_v<T, Ts> || ...);

	template<typename T, typename... Ts>
	concept any_of_t = any_of_v<T, Ts...>;

	template<typename T, typename... Ts>
	constexpr static bool all_of_v = (std::is_same_v<T, Ts> && ...);

	template<typename T, typename... Ts>
	concept all_of_t = all_of_v<T, Ts...>;
}// namespace gal::prometheus::inline infrastructure::concepts

// https://github.com/Life4gal/prometheus/blob/master/include/prometheus/infrastructure/aligned_union.hpp

#if defined(GAL_INI_PLATFORM_WINDOWS)
#define GAL_PROMETHEUS_UNREACHABLE() __assume(0)
#define GAL_PROMETHEUS_DEBUG_TRAP() __debugbreak()
#define GAL_PROMETHEUS_IMPORTED_SYMBOL __declspec(dllimport)
#define GAL_PROMETHEUS_EXPORTED_SYMBOL __declspec(dllexport)
#define GAL_PROMETHEUS_LOCAL_SYMBOL

#define GAL_PROMETHEUS_DISABLE_WARNING_PUSH __pragma(warning(push))
#define GAL_PROMETHEUS_DISABLE_WARNING_POP __pragma(warning(pop))
#define GAL_PROMETHEUS_DISABLE_WARNING(warningNumber) __pragma(warning(disable \
																		   : warningNumber))
#elif defined(GAL_INI_PLATFORM_LINUX)
	#define GAL_PROMETHEUS_UNREACHABLE() __builtin_unreachable()
	#define GAL_PROMETHEUS_DEBUG_TRAP() __builtin_trap()
	#define GAL_PROMETHEUS_IMPORTED_SYMBOL __attribute__((visibility("default")))
	#define GAL_PROMETHEUS_EXPORTED_SYMBOL __attribute__((visibility("default")))
	#define GAL_PROMETHEUS_LOCAL_SYMBOL __attribute__((visibility("hidden")))

	#define GAL_PROMETHEUS_DISABLE_WARNING_PUSH _Pragma("GCC diagnostic push")
	#define GAL_PROMETHEUS_DISABLE_WARNING_POP _Pragma("GCC diagnostic pop")

	#define GAL_PROMETHEUS_PRIVATE_DO_PRAGMA(X) _Pragma(#X)
	#define GAL_PROMETHEUS_DISABLE_WARNING(warningName) GAL_PROMETHEUS_PRIVATE_DO_PRAGMA(GCC diagnostic ignored #warningName)
#elif defined(GAL_INI_PLATFORM_MACOS)
	#define GAL_PROMETHEUS_UNREACHABLE() __builtin_unreachable()
	#define GAL_PROMETHEUS_DEBUG_TRAP() __builtin_trap()
	#define GAL_PROMETHEUS_IMPORTED_SYMBOL __attribute__((visibility("default")))
	#define GAL_PROMETHEUS_EXPORTED_SYMBOL __attribute__((visibility("default")))
	#define GAL_PROMETHEUS_LOCAL_SYMBOL __attribute__((visibility("hidden")))

	#define GAL_PROMETHEUS_DISABLE_WARNING_PUSH _Pragma("clang diagnostic push")
	#define GAL_PROMETHEUS_DISABLE_WARNING_POP _Pragma("clang diagnostic pop")

	#define GAL_PROMETHEUS_PRIVATE_DO_PRAGMA(X) _Pragma(#X)
	#define GAL_PROMETHEUS_DISABLE_WARNING(warningName) GAL_PROMETHEUS_PRIVATE_DO_PRAGMA(clang diagnostic ignored #warningName)
#else
	#define GAL_PROMETHEUS_UNREACHABLE()
	#define GAL_PROMETHEUS_DEBUG_TRAP()
	#define GAL_PROMETHEUS_IMPORTED_SYMBOL
	#define GAL_PROMETHEUS_EXPORTED_SYMBOL
	#define GAL_PROMETHEUS_LOCAL_SYMBOL

	#define GAL_PROMETHEUS_DISABLE_WARNING_PUSH
	#define GAL_PROMETHEUS_DISABLE_WARNING_POP
	#define GAL_PROMETHEUS_DISABLE_WARNING(warningName)
#endif

#if defined(GAL_PROMETHEUS_COMPILER_MSVC)
	#define GAL_PROMETHEUS_DISABLE_WARNING_MSVC(...) GAL_PROMETHEUS_DISABLE_WARNING(__VA_ARGS__)
#else
#define GAL_PROMETHEUS_DISABLE_WARNING_MSVC(...)
#endif

#if defined(GAL_PROMETHEUS_COMPILER_GNU)
	#define GAL_PROMETHEUS_DISABLE_WARNING_GNU(...) GAL_PROMETHEUS_DISABLE_WARNING(__VA_ARGS__)
#else
#define GAL_PROMETHEUS_DISABLE_WARNING_GNU(...)
#endif

#if defined(GAL_PROMETHEUS_COMPILER_APPLE_CLANG) || defined(GAL_PROMETHEUS_COMPILER_CLANG_CL) || defined(GAL_PROMETHEUS_COMPILER_CLANG)
	#define GAL_PROMETHEUS_DISABLE_WARNING_CLANG(...) GAL_PROMETHEUS_DISABLE_WARNING(__VA_ARGS__)
#else
#define GAL_PROMETHEUS_DISABLE_WARNING_CLANG(...)
#endif

#include <memory>

namespace gal::prometheus::inline infrastructure
{
	template<typename... Ts>
		requires(not std::is_reference_v<Ts> && ...)
	class AlignedUnion final
	{
	public:
		constexpr static std::size_t max_size      = std::size_t{functor::max(sizeof(Ts)...)};
		constexpr static std::size_t max_alignment = functor::max(alignof(Ts)...);

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		struct constructor_tag { };

	private:
		alignas(max_alignment) unsigned char data_[max_size]{};

	public:
		constexpr AlignedUnion() noexcept = default;

		template<typename T, typename... Args>
			requires std::is_constructible_v<T, Args...>
		constexpr explicit AlignedUnion(constructor_tag<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) { this->template store<T>(std::forward<Args>(args)...); }

		constexpr AlignedUnion(const AlignedUnion&) noexcept((std::is_nothrow_copy_constructible_v<Ts> && ...))
			requires(std::is_copy_constructible_v<Ts> && ...)
		= default;
		constexpr auto operator=(const AlignedUnion&) noexcept((std::is_nothrow_copy_assignable_v<Ts> && ...)) -> AlignedUnion& requires(std::is_copy_assignable_v<Ts> && ...)
		= default;
		// constexpr      AlignedUnion(const AlignedUnion&)               = delete;
		// constexpr auto operator=(const AlignedUnion&) -> AlignedUnion& = delete;

		constexpr AlignedUnion(AlignedUnion&&) noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
			requires(std::is_move_constructible_v<Ts> && ...)
		= default;
		constexpr auto operator=(AlignedUnion&&) noexcept((std::is_nothrow_move_assignable_v<Ts> && ...)) -> AlignedUnion& requires(std::is_move_assignable_v<Ts> && ...)
		= default;
		// constexpr      AlignedUnion(AlignedUnion&&)               = delete;
		// constexpr auto operator=(AlignedUnion&&) -> AlignedUnion& = delete;

		constexpr ~AlignedUnion() noexcept = default;

		template<typename T, typename... Args>
			requires concepts::any_of_t<T, Ts...> and std::is_constructible_v<T, Args...>
		constexpr auto store(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) { std::construct_at(reinterpret_cast<T*>(&data_), std::forward<Args>(args)...); }

		// Note: If the type saved by AlignedUnion has a non-trivial destructor but no destroy is called, leaving AlignedUnion to destruct or calling store (if it already has a value) will be undefined behavior! (maybe memory or resource leak)
		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		constexpr auto destroy() noexcept(std::is_nothrow_destructible_v<T>) -> void { std::destroy_at(reinterpret_cast<T*>(&data_)); }

		template<typename New, typename Old, typename... Args>
			requires concepts::any_of_t<New, Ts...> and concepts::any_of_t<Old, Ts...> and std::is_constructible_v<New, Args...>
		constexpr auto exchange(Args&&... args) noexcept(std::is_nothrow_constructible_v<New, Args...> and std::is_nothrow_move_constructible_v<Old>) -> Old
		{
			auto&& old = std::move(this->template load<Old>());
			this->template store<New>(std::forward<Args>(args)...);
			return old;
		}

		template<typename New, typename Old, typename... Args>
			requires concepts::any_of_t<New, Ts...> and concepts::any_of_t<Old, Ts...> and std::is_constructible_v<New, Args...>
		constexpr auto replace(Args&&... args) noexcept(std::is_nothrow_constructible_v<New, Args...> and std::is_nothrow_destructible_v<Old>) -> void
		{
			this->template destroy<Old>();
			this->template store<New>(std::forward<Args>(args)...);
		}

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr auto load() noexcept -> T& { return *std::launder(reinterpret_cast<T*>(data_)); }

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr auto load() const noexcept -> const T& { return *std::launder(reinterpret_cast<const T*>(data_)); }

		// Compare pointers, returns true iff both AlignedUnion store the same pointer.
		[[nodiscard]] constexpr auto operator==(const AlignedUnion& other) const noexcept -> bool
		{
			GAL_PROMETHEUS_DISABLE_WARNING_PUSH
			GAL_PROMETHEUS_DISABLE_WARNING_CLANG(-Wundefined-reinterpret-cast)

			return *reinterpret_cast<const void* const*>(&data_) == *reinterpret_cast<const void* const*>(&other.data_);

			GAL_PROMETHEUS_DISABLE_WARNING_POP
		}

		template<typename T>
			requires concepts::any_of_t<T, Ts...> and std::is_copy_constructible_v<T>
		[[nodiscard]] constexpr explicit operator T() const noexcept(std::is_nothrow_copy_constructible_v<T>) { return load<T>(); }

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr explicit operator T&() noexcept { return load<T>(); }

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr explicit operator const T&() const noexcept { return load<T>(); }

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr auto equal(const AlignedUnion& other) const noexcept(noexcept(std::declval<const T&>() == std::declval<const T&>())) -> bool { return load<T>() == other.template load<T>(); }

		template<typename T>
			requires concepts::any_of_t<T, Ts...>
		[[nodiscard]] constexpr auto equal(const T& other) const noexcept(noexcept(std::declval<const T&>() == std::declval<const T&>())) -> bool { return load<T>() == other; }
	};
}// namespace gal::prometheus::inline infrastructure

// https://github.com/Life4gal/prometheus/blob/master/include/prometheus/infrastructure/function_ref.hpp

#include <cassert>
#define GAL_PROMETHEUS_DEBUG_NOT_NULL(p, message) assert((p) && (message))

#include <functional>
#include <tuple>

namespace gal::prometheus::inline infrastructure
{
	template<typename Signature>
	class FunctionRef;

	template<typename>
	struct is_function_ref : std::false_type { };

	template<typename Signature>
	struct is_function_ref<FunctionRef<Signature>> : std::true_type { };

	template<typename T>
	constexpr static auto is_function_ref_v = is_function_ref<T>::value;

	template<typename T>
	concept function_ref_t = is_function_ref_v<T>;

	namespace function_ref_detail
	{
		template<typename>
		struct compatible_function_pointer
		{
			using type = void;

			template<typename...>
			constexpr static auto value = false;
		};

		template<typename FunctionPointer>
			requires std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<std::decay_t<FunctionPointer>>>>
		struct compatible_function_pointer<FunctionPointer>
		{
			using type = std::decay_t<FunctionPointer>;

			template<typename Return, typename... Args>
			constexpr static auto value = std::is_invocable_v<type, Args...> and (std::is_void_v<Return> or std::is_convertible_v<std::invoke_result_t<type, Args...>, Return>);
		};
	}// namespace function_ref_detail

	/**
	 * @brief A reference to a function.
	 * This is a lightweight reference to a function.
	 * It can refer to any function that is compatible with given signature.
	 *
	 * A function is compatible if it is callable with regular function call syntax from the given argument types,
	 * and its return type is either implicitly convertible to the specified return type or the specified return type is `void`.
	 *
	 * In general it will store a pointer to the functor, requiring an lvalue.
	 * But if it is created with a function pointer or something convertible to a function pointer,
	 * it will store the function pointer itself.
	 */
	template<typename Return, typename... Args>
	class FunctionRef<Return(Args...)>
	{
	public:
		using result_type = Return;
		using signature = Return(Args...);

		using arguments = std::tuple<Args...>;
		constexpr static auto argument_size = std::tuple_size_v<arguments>;
		template<std::size_t Index>
		using argument_type = std::tuple_element_t<Index, arguments>;

	private:
		using function_pointer = void (*)();
		using functor_pointer = void*;

		using data_type = AlignedUnion<function_pointer, functor_pointer>;

		using invoker = result_type (*)(data_type, Args...);

		struct constructor_tag { };

		data_type data_;
		invoker   invoker_;

		template<typename FunctionType>
		constexpr static auto do_invoke_function(
				data_type data,
				Args...   args) noexcept(noexcept(static_cast<result_type>(std::invoke(reinterpret_cast<FunctionType>(data.load<function_pointer>()), static_cast<Args>(args)...))))// NOLINT(clang-diagnostic-cast-function-type-strict)
			-> result_type
		{
			auto pointer  = data.load<function_pointer>();
			auto function = reinterpret_cast<FunctionType>(pointer);// NOLINT(clang-diagnostic-cast-function-type-strict)

			return static_cast<result_type>(std::invoke(function, static_cast<Args>(args)...));
		}

		template<typename Functor>
		constexpr static auto do_invoke_functor(
				data_type data,
				Args...   args) noexcept(noexcept(static_cast<result_type>(std::invoke(*static_cast<Functor*>(data.load<functor_pointer>()), static_cast<Args>(args)...))))
			-> result_type
		{
			auto  pointer = data.load<functor_pointer>();
			auto& functor = *static_cast<Functor*>(pointer);

			return static_cast<result_type>(std::invoke(functor, static_cast<Args>(args)...));
		}

		template<typename FunctionPointer>
		constexpr FunctionRef(constructor_tag, FunctionPointer function) noexcept
			: data_{data_type::constructor_tag<function_pointer>{}, reinterpret_cast<function_pointer>(function)}// NOLINT(clang-diagnostic-cast-function-type-strict)
			,
			invoker_{&do_invoke_function<typename function_ref_detail::compatible_function_pointer<FunctionPointer>::type>}
		{
			// throw exception?
			GAL_PROMETHEUS_DEBUG_NOT_NULL(function, "function pointer must not be null");
		}

	public:
		template<
			typename FunctionPointer>
			requires function_ref_detail::compatible_function_pointer<FunctionPointer>::template
			value<Return, Args...>
		constexpr explicit(false) FunctionRef(FunctionPointer function) noexcept(std::is_nothrow_constructible_v<FunctionRef, constructor_tag, FunctionPointer>)
			: FunctionRef{constructor_tag{}, function} { }

		/**
		 * @brief Creates a reference to the function created by the stateless lambda.
		 */
		template<typename StatelessLambda>
			requires(not function_ref_detail::compatible_function_pointer<StatelessLambda>::template value<Return, Args...>) and
					std::is_invocable_v<StatelessLambda, Args...> and
					(std::is_void_v<Return> or std::is_convertible_v<std::invoke_result_t<StatelessLambda, Args...>, Return>) and
					requires(StatelessLambda& functor)
					{
						(+functor)(std::declval<Args>()...);
					}
		// note: const reference to avoid ambiguous
		constexpr explicit(false) FunctionRef(const StatelessLambda& functor)
			: FunctionRef{constructor_tag{}, +functor} { }

		/**
		 * @brief Creates a reference to the specified functor.
		 * It will store a pointer to the function object, so it must live as long as the reference.
		 */
		template<typename Functor>
			requires(not function_ref_detail::compatible_function_pointer<Functor>::template value<Return, Args...>) and
					(not is_function_ref_v<Functor>) and
					std::is_invocable_v<Functor, Args...> and
					(std::is_void_v<Return> or std::is_convertible_v<std::invoke_result_t<Functor, Args...>, Return>)
		constexpr explicit(false) FunctionRef(Functor& functor) noexcept
			: data_{data_type::constructor_tag<functor_pointer>{}, const_cast<functor_pointer>(static_cast<const void*>(&functor))},
			invoker_{&do_invoke_functor<Functor>} { }

		constexpr auto operator()(Args... args) noexcept(noexcept(std::invoke(invoker_, data_, static_cast<Args>(args)...))) { return std::invoke(invoker_, data_, static_cast<Args>(args)...); }
	};
}// namespace gal::prometheus::inline infrastructure

namespace gal::ini
{
	using prometheus::FunctionRef;
	using prometheus::is_function_ref;
	using prometheus::function_ref_t;
}// namespace gal::ini
