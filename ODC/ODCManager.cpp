//
//  ODCManager.cpp
//  opendatacon_suite
//
//  Created by Alan Murray on 6/06/2017.
//
//

#include <opendatacon/ODCManager.h>
#include <iostream>

namespace odc {
	ODCManager::ODCManager(
				  uint32_t concurrencyHint,
				  openpal::ICryptoProvider* crypto,
				  std::function<void()> onThreadStart,
				  std::function<void()> onThreadExit
			   ) :
	ios_working(new asio::io_service::work(IOS)),
	scheduler(IOS)
	{
		for (size_t i = 0; i < concurrencyHint; ++i)
			std::thread([&](){
				for (;;) {
					try {
						IOS.run();
						break;
					} catch (std::exception& e) {
						std::cout << "Exception: " << e.what() << std::endl;
						// TODO: work out what best to do
						// log exception
						// shutdown port, restart application?
					}
					
				}
			}).detach();
	}

}
