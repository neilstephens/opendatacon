#include "TCPSocketManager.h"

namespace odc
{

//implement a std::stringbuf with a TCP connection
//for use in a std::ostream
class TCPstringbuf: public std::stringbuf
{
public:
	void Init(asio::io_service* apIOS,
		bool aisServer,
		const std::string& aEndPoint,
		const std::string& aPort
		)
	{
		pIOS = apIOS;
		pSockMan = std::make_unique<TCPSocketManager<std::string>>(apIOS,aisServer,aEndPoint,aPort,
			[](buf_t& readbuf){},[](bool state){},true);
		pSockMan->Open();
	}
	void DeInit()
	{
		if(pSockMan)
			pSockMan->Close();
	}
	int sync() override
	{
		if(pSockMan)
		{
			pSockMan->Write(str()); //write
			str("");                //clear
			return 0;               //success
		}
		return -1; //fail
	}
private:
	asio::io_service* pIOS;
	std::unique_ptr<TCPSocketManager<std::string>> pSockMan;
};

} //namespace odc
