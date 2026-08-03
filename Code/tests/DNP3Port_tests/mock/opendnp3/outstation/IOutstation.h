#ifndef OPENDNP3_IOUTSTATION_H
#define OPENDNP3_IOUTSTATION_H

#include "opendnp3/outstation/UpdateBuilder.h"

namespace opendnp3
{

class IOutstation
{
public:
	virtual ~IOutstation() = default;
	virtual void Apply(const Updates& updates) = 0;
};

} // namespace opendnp3

#endif
