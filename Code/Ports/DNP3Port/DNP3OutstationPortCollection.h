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
		this->AddCommand("IINRestart", [this](const ParamCollection &params) -> const Json::Value
			{
				//one param - string with flags
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No DNP3OutstationPort matched");
				target->pOutstation->SetRestartIIN();
				return IUIResponder::GenerateResult("Success");
			},"Simulate a device restart by setting the stack IIN flag. Usage: IINRestart <port_regex>");
		this->AddCommand("AdjustTimeOffsetMilliSeconds", [this](const ParamCollection &params) -> const Json::Value
			{
				auto target = GetTarget(params).lock();
				if(!target)
					return IUIResponder::GenerateResult("No DNP3OutstationPort matched");
				//check params
				int64_t offset = 0; bool sync_upstream = false;
				try
				{
					if(params.count("0") == 0)
						throw std::runtime_error("Too few args");
					offset = std::stoll(params.at("0"));
					if(params.count("1"))
						sync_upstream = !(odc::to_lower(params.at("1"))=="false" || params.at("1")=="0");
				}
				catch(const std::exception& e)
				{
					return IUIResponder::GenerateResult(std::string("Bad parameter: ")+e.what());
				}
				target->AdjustTimeOffsetMilliSeconds(offset,sync_upstream);
				return IUIResponder::GenerateResult("Success");
			},"Set the internal offset used when locally timestamping events. Optionally pass time sync event up-stream. Usage: AdjustTimeOffsetMilliSeconds <port_regex> <ms_offset> [sync_upstream=false]");
	}
	virtual ~DNP3OutstationPortCollection(){}
};

#endif // DNP3OUTSTATIONPORTCOLLECTION_H
