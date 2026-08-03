#ifndef OPENDNP3_UPDATEBUILDER_H
#define OPENDNP3_UPDATEBUILDER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace opendnp3
{

enum class EventMode { Detect, Force, Suppress };

using Updates = std::vector<std::function<void ()>>;

class UpdateBuilder
{
public:
	template<typename T>
	bool Update(T, uint16_t, EventMode) { updates.push_back([](){}); return true; }

	Updates Build() { return std::move(updates); }

private:
	Updates updates;
};

} // namespace opendnp3

#endif
