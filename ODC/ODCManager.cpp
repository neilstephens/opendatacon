//
//  ODCManager.cpp
//  opendatacon_suite
//
//  Created by Alan Murray on 6/06/2017.
//
//

#include <opendatacon/ODCManager.h>

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
			std::thread([&](){ IOS.run(); }).detach();
	}

}
