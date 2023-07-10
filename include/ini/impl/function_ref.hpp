// Copyright (C) 2022-2023 Life4gal <life4gal@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#pragma once

#include <memory>
#include <type_traits>

#if defined(DEBUG) or defined(_DEBUG)
#include <cassert>
#define GAL_INI_FUNCTION_REF_DEBUG
#endif

namespace gal::ini
{
	template<typename Signature>
	class FunctionRef;

	/// A reference to a function.
	///
	/// This is a lightweight reference to a function.
	/// It can refer to any function that is compatible with given signature.
	///
	/// A function is compatible if it is callable with regular function call syntax from the given
	/// argument types, and its return type is either implicitly convertible to the specified return
	/// type or the specified return type is `void`.
	///
	/// In general it will store a pointer to the functor, requiring an lvalue.
	/// But if it is created with a function pointer or something convertible to a function pointer,
	/// it will store the function pointer itself.
	/// 
	/// \notes Due to implementation reasons, it does not support member function pointers,
	/// as it requires regular function call syntax.
	/// Create a reference to the object returned by [std::mem_fn](), if that is required.
	template<typename Return, typename... Args>
	class FunctionRef<Return(Args...)>
	{
	public:
		using result_type = Return;
		using signature = Return(Args...);

	private:
		using pointer = Return (*)(Args...);
		using invoker = result_type(*)(const void*, Args...);

		struct tag {};

		alignas(alignof(pointer)) unsigned char data_[alignof(pointer)];
		invoker                                 invoker_;

		template<typename FunctionType, typename DataType>
		constexpr static auto do_invoke_function(
				const void* data,
				Args...     args
				) noexcept(noexcept(static_cast<result_type>(std::invoke(reinterpret_cast<FunctionType>(*static_cast<const DataType*>(data)), static_cast<Args>(args)...))))
			-> result_type
		{
			auto d       = *static_cast<const DataType*>(data);
			auto functor = reinterpret_cast<FunctionType>(d);
			return static_cast<result_type>(std::invoke(functor, static_cast<Args>(args)...));
		}

		template<typename Functor, typename DataType = void*>
		constexpr static auto do_invoke_functor(
				const void* data,
				Args...     args
				) noexcept(noexcept(static_cast<result_type>(std::invoke(*static_cast<Functor*>(*static_cast<const DataType*>(data)), static_cast<Args>(args)...))))
			-> result_type
		{
			auto  d       = *static_cast<const DataType*>(data);
			auto& functor = *static_cast<Functor*>(d);
			return static_cast<result_type>(std::invoke(functor, static_cast<Args>(args)...));
		}

		template<typename R, typename... As>
		constexpr FunctionRef(tag, R (*function)(As...)) noexcept
			: data_{}
		{
			using function_type = R(*)(As...);
			using data_type = void(*)();

			// no empty check!
			#if defined(GAL_INI_FUNCTION_REF_DEBUG)
			assert(function && "function pointer must not be null");
			#endif

			std::construct_at(reinterpret_cast<data_type>(&data_), reinterpret_cast<data_type>(function));
			invoker_ = &do_invoke_function<function_type, data_type>;
		}

	public:
		constexpr explicit FunctionRef(Return (*function)(Args...))
			noexcept(std::is_nothrow_constructible_v<FunctionRef, tag, decltype(function)>)
			: FunctionRef{tag{}, function} {}

		template<typename CompatibleReturn>
			requires std::is_void_v<Return> or std::is_convertible_v<CompatibleReturn, Return>
		constexpr explicit FunctionRef(CompatibleReturn (*function)(Args...))
			noexcept(std::is_nothrow_constructible_v<FunctionRef, tag, decltype(function)>)
			: FunctionRef{tag{}, function} { }

		/// \effects Creates a reference to the function created by the stateless lambda.
		template<typename Functor>
			requires
			std::is_invocable_v<Functor, Args...> and
			(std::is_void_v<Return> or std::is_convertible_v<std::invoke_result_t<Functor, Args...>, Return>) and
			requires(Functor& functor)
			{
				(+functor)(std::declval<Args>()...);
			}
		constexpr explicit(false) FunctionRef(const Functor& functor)
			: FunctionRef{tag{}, +functor} {}

		/// \effects Creates a reference to the specified functor.
		/// It will store a pointer to the function object, so it must live as long as the reference.
		template<typename Functor>
			requires
			std::is_invocable_v<Functor, Args...> and
			(std::is_void_v<Return> or std::is_convertible_v<std::invoke_result_t<Functor, Args...>, Return>)
		constexpr explicit(false) FunctionRef(Functor& functor) noexcept
			: data_{},
			invoker_{&do_invoke_functor<Functor>} { std::construct_at(reinterpret_cast<void*>(&data_), &functor); }

		constexpr auto operator()(Args... args) noexcept(noexcept(std::invoke(invoker_, reinterpret_cast<void*>(&data_), static_cast<Args>(args)...))) { return std::invoke(invoker_, reinterpret_cast<void*>(&data_), static_cast<Args>(args)...); }
	};
}
