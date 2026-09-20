/*	opendatacon
*
*	Copyright (c) 2015:
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
/**
*/

#ifndef PORTLOADER_H
#define PORTLOADER_H

#include <opendatacon/DataPort.h>
#include <opendatacon/Platform.h>
#include <unordered_map>

using namespace odc;

typedef DataPort* (*newptr)(const std::string& Name, const std::string& File, const Json::Value& Overrides);
typedef void (*delptr)(DataPort*);

symbol_ptr GetPortFunc(module_ptr pluginlib, const std::string& objname, bool destroy = false);
newptr GetPortCreator(module_ptr pluginlib, const std::string& objname);
delptr GetPortDestroyer(module_ptr pluginlib, const std::string& objname);

// Unloading and reloading a plugin dozens of times per test binary (one
// LoadModule()/UnLoadModule() pair per TEST_CASE) risks a still-in-flight
// callback on the shared io_service later calling into memory that's no
// longer mapped once the library is freed - sporadic, inconsistent crashes
// with no fixed signature. Load each library once per process and never
// really unload it; the OS cleans up at exit. Redirects the plain
// LoadModule()/UnLoadModule() calls already used throughout the test suites.
inline module_ptr CachedLoadModule(const std::string& path, bool global = false)
{
	static std::unordered_map<std::string, module_ptr> cache;
	auto it = cache.find(path);
	if(it != cache.end())
		return it->second;
	auto handle = LoadModule(path, global);
	if(handle)
		cache.emplace(path, handle);
	return handle;
}
inline bool CachedUnLoadModule(module_ptr)
{
	return true; //no-op - see CachedLoadModule() above
}
#define LoadModule CachedLoadModule
#define UnLoadModule CachedUnLoadModule

#endif
