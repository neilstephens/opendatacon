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
 * asio.h
 *
 *  Created on: 2018-01-28
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

#ifndef ASIO_H
#define ASIO_H

#define ASIO_HAS_STD_ADDRESSOF
#define ASIO_HAS_STD_ARRAY
#define ASIO_HAS_CSTDINT
#define ASIO_HAS_STD_TYPE_TRAITS
#define ASIO_HAS_STD_SHARED_PTR

#include <asio.hpp>

//use these to suppress warnings
typedef struct
{
	const asio::error_category& use1 = asio::error::system_category;
	const asio::error_category& use2 = asio::error::netdb_category;
	const asio::error_category& use3 = asio::error::addrinfo_category;
	const asio::error_category& use4 = asio::error::misc_category;
}use_these_t;

#endif // ASIO_H

