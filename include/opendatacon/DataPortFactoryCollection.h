/*	opendatacon
 *
 *	Copyright (c) 2017:
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
//
//  DataPortFactoryCollection.h
//  opendatacon
//
//  Created by Alan Murray on 09/05/2017.
//
//

#ifndef __opendatacon__DataPortFactoryCollection__
#define __opendatacon__DataPortFactoryCollection__

#include "DataPortFactory.h"
#include "ResponderMap.h"

#include "Platform.h"

namespace odc
{	
	class DataPortFactoryCollection: public ResponderMap<DataPortFactory>
	{
	public:
		DataPortFactoryCollection(std::shared_ptr<IOManager> IOMgr) : IOMgr_(IOMgr) {}
		virtual ~DataPortFactoryCollection(){}
		
		DataPortFactory* GetFactory(const std::string FactoryName) {
			
			if (this->count(FactoryName)) {
				// Already have a factory, return it
				return this->at(FactoryName).get();
			}
			//otherwise try and find the factory
			
			std::string libname = GetLibFileName(FactoryName);
			auto* portlib = LoadModule(libname.c_str());
			
			if(portlib == nullptr)
			{
				std::cout << "Warning: failed to load library '"<<libname<<"' for port type '"<<FactoryName<<"'"<<std::endl;
				return nullptr;
			}
			
			// API says that libraries should export a factory creation function: new_DataPortPortFactory(odc::IOManager)
			const std::string new_factoryname = "new_DataPortFactory";
			auto new_portfactory_func = (DataPortFactory*(*)(std::shared_ptr<odc::IOManager>))LoadSymbol(portlib, new_factoryname);
			if(new_portfactory_func == nullptr)
			{
				std::cout << "Warning: failed to load symbol '"<<new_factoryname<<"' for port type '"<<FactoryName<<"'"<<std::endl;
				return nullptr;
			}
			
			// API says that libraries should export a factory deletion function: delete_DataPortPortFactory(odc::IOManager)
			const std::string delete_factoryname = "delete_DataPortFactory";
			auto delete_factory_func = (void(*)(DataPortFactory*))LoadSymbol(portlib, delete_factoryname);
			if(delete_factory_func == nullptr)
			{
				std::cout << "Warning: failed to load symbol '"<<delete_factoryname<<"' for port type '"<<FactoryName<<"'"<<std::endl;
				return nullptr;
			}
			
			//call the creation function and wrap the returned pointer to a new port
			DataPortFactory* factory = new_portfactory_func(IOMgr_);
			if(factory == nullptr)
			{
				std::cout << "Warning: failed construct factory for port type '"<<FactoryName<<"'"<<std::endl;
				return nullptr;
			}

			this->emplace(FactoryName, std::unique_ptr<DataPortFactory,void(*)(DataPortFactory*)>(factory,delete_factory_func));
			return factory;
		}
	private:
		std::shared_ptr<IOManager> IOMgr_;
	};
}

#endif /* defined(__opendatacon__DataPortFactoryCollection__) */
