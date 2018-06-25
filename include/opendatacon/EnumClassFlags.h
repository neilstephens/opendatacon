/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * EnumClassFlags.h
 *
 *  Created on: 23/06/2018
 *      Author: Neil Stephens <dearknarl@gmail.com>
 *
 * Inspired by this blog on typesafe bitmasks:
 *	http://blog.bitwigglers.org/using-enum-classes-as-type-safe-bitmasks/
 *
 */

#ifndef ENUMCLASSFLAGS_H
#define ENUMCLASSFLAGS_H

#include <type_traits>

namespace odc
{

//A default template to disallow bitwise operations on enums classes that aren't flags
template<typename E>
struct BitwiseEnabled
{
	static const bool state = false;
};

//Macro to specialise the above template for enum classes that are flags
#define ENABLE_BITWISE(E)                   \
	template<>                            \
	struct BitwiseEnabled<E>              \
	{                                     \
		static const bool state = true; \
	};

//templates for bitwise binary-operators locked to bitwise enabled enum classes
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator & ( const E lhs, const E rhs)
{
	return static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) & static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator | ( const E lhs, const E rhs)
{
	return static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) | static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator ^ ( const E lhs, const E rhs)
{
	return static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) ^ static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator &= ( E& lhs, const E rhs)
{
	return lhs = static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) & static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator |= ( E& lhs, const E rhs)
{
	return lhs = static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) | static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator ^= ( E& lhs, const E rhs)
{
	return lhs = static_cast<E>(static_cast<std::underlying_type_t<E> >(lhs) ^ static_cast<std::underlying_type_t<E> >(rhs));
}
template <typename E>
constexpr std::enable_if_t<BitwiseEnabled<E>::state, E> operator ~( const E arg)
{
	return static_cast<E>(~static_cast<std::underlying_type_t<E> >(arg));
}

} //namespace odc

#endif //ENUMCLASSFLAGS_H
