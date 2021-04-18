#ifndef DNP3OUTSTATIONPORTCOLLECTION_H
#define DNP3OUTSTATIONPORTCOLLECTION_H

#include "DNP3OutstationPort.h"
#include <opendatacon/DataPort.h>
#include <opendatacon/ResponderMap.h>

using namespace odc;

class DNP3OutstationPortCollection: public ResponderMap< std::weak_ptr<DNP3OutstationPort> >
{
public:
	DNP3OutstationPortCollection()
	{
		this->AddCommand("SetIINFlags", [this](const ParamCollection &params) -> const Json::Value
			{
				//one param - string with flags
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No DNP3OutstationPort matched");
				//check mandatory params are there
				if(params.count("0") == 0 || FromString(params.at("0")) == AppIINFlags::NONE)
				{
				      return IUIResponder::GenerateResult("Bad parameter");
				}
				target->SetIINFlags(params.at("0"));
				return IUIResponder::GenerateResult("Success");
			},"Set the outstation application level IIN flags specified. Usage: SetIINFlags <port_regex> <[sub]set of NEED_TIME|LOCAL_CONTROL|DEVICE_TROUBLE|CONFIG_CORRUPT|EVENT_BUFFER_OVERFLOW>");
		this->AddCommand("ClearIINFlags", [this](const ParamCollection &params) -> const Json::Value
			{
				//one param - string with flags
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No DNP3OutstationPort matched");
				//check mandatory params are there
				if(params.count("0") == 0 || FromString(params.at("0")) == AppIINFlags::NONE)
				{
				      return IUIResponder::GenerateResult("Bad parameter");
				}
				target->ClearIINFlags(params.at("0"));
				return IUIResponder::GenerateResult("Success");
			},"Clear the outstation application level IIN flags specified. Usage: ClearIINFlags <port_regex> <[sub]set of NEED_TIME|LOCAL_CONTROL|DEVICE_TROUBLE|CONFIG_CORRUPT|EVENT_BUFFER_OVERFLOW>");
		this->AddCommand("ShowIINFlags", [this](const ParamCollection &params) -> const Json::Value
			{
				//one param - string with flags
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No DNP3OutstationPort matched");
				return IUIResponder::GenerateResult(ToString(target->IINFlags));
			},"Show the current outstation application level IIN flags. Usage: ShowIINFlags <port_regex>");
	}
	void Add(std::shared_ptr<DNP3OutstationPort> p, const std::string& Name)
	{
		std::lock_guard<std::mutex> lck(mtx);
		this->insert(std::pair<std::string,std::shared_ptr<DNP3OutstationPort>>(Name,p));
	}
	virtual ~DNP3OutstationPortCollection(){}
private:
	std::mutex mtx;
};

#endif // DNP3OUTSTATIONPORTCOLLECTION_H
