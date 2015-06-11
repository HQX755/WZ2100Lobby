#ifndef _NET_H_
#define _NET_H_

#include "stdafx.h"
#include "Asio.h"
#include "Helper.h"

#include <boost/unordered_set.hpp>

#define MAX_RECV	512
#define MAX_SEND	512
#define WZ_PORT		9990

#define HANDLER_NUM 2
#define NET_APPEND 4
#define READ_HANDLER 1
#define WRITE_HANDLER 0
#define NET_WRITE 3

class CUser;

class CNet
{
private:
	boost::asio::io_service &m_Service;
	boost::asio::strand m_Strand;
	boost::asio::ip::tcp::socket m_Socket;

	boost::asio::streambuf m_InputBuffer; //unused
	boost::asio::streambuf m_OutputBuffer; //unused

	std::deque<std::pair<uint8_t *, uint32_t>> m_OutputQueue;
	
	uint8_t m_InputData[MAX_RECV];
	uint8_t m_OutputData[MAX_SEND];

	size_t m_LastOutputSize;
	size_t m_LastInputSize;

	size_t m_PacketsSent;
	size_t m_PacketsReceived;

	bool bInterrupt;
	bool bHandlerActive[4];

	handler_allocator allocator_;

	CUser *m_User;

public:
	CNet(boost::asio::io_service &service) : 
		m_Service(service),
		m_Strand(service),
		m_Socket(service),
		m_LastOutputSize(0),
		m_LastInputSize(0),
		m_PacketsReceived(0),
		m_PacketsSent(0),
		bInterrupt(false), 
		m_User(NULL)
	{
		Initialize();
	}

	void Initialize()
	{
		memset(&bHandlerActive, 0, sizeof(bool) * HANDLER_NUM); 
		memset(&m_OutputData, 0, MAX_SEND);
	}

	~CNet();

	boost::asio::ip::tcp::socket &GetSocket();

	void Update();
	void AddData(uint8_t *data, uint32_t size, bool add = false, bool allocated = true);

	void Read();
	void ReadHandler(const boost::system::error_code& ec, std::size_t s);

	void Start();
	void Stop();

	void BindUser(CUser *user);

	void DeleteLater()
	{
		bInterrupt = true;
		m_User = NULL;
	}

	CUser *GetUser();

private:
	void DoWrite(uint8_t *data, uint32_t size, bool add = false, bool alloacted = false);
	void Write(bool add = false, bool allocated = false);
	void WriteHandler(boost::system::error_code ec, std::size_t s, bool allocated = false);

	void WaitForHandlerCompletion();
};

class CListener : public CInstance<CListener>
{
private:
	boost::asio::io_service &MainService;
	boost::asio::ip::tcp::acceptor Acceptor;

	CNet *NET;
	handler_allocator m_Allocator;

public:
	CListener(boost::asio::io_service &main) : MainService(main), 
		Acceptor(main, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), WZ_PORT)),
		NET(NULL)
	{
		Initialize();
	}

	~CListener()
	{
		if(NET != NULL)
		{
			delete NET;
			NET = NULL;
		}
	}

	void Initialize();
	void Start();

private:
	void HandleAccept(const boost::system::error_code &ec);
};

#endif