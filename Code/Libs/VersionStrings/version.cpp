#include "version.h"
#include <string>

namespace odc
{

std::string version_string()
{
	return ODC_VERSION_STRING;
}

std::string submodules_version_string()
{
	return ODC_VERSION_SUBMODULES;
}

} //namespace odc
