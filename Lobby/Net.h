#ifndef _NET_H_
#define _NET_H_

#include "stdafx.h"
#include "Asio.h"
#include "Helper.h"

#define MAX_RECV 512
#define MAX_SEND 512

class CUser;

class CNet
{
private:
	boost::asio::io_service &m_Service;
	boost::asio::ip::tcp::socket m_Socket;

	boost::asio::streambuf m_InputBuffer; //unused
	boost::asio::streambuf m_OutputBuffer; //unused

	//std::vector<uint8_t> m_OutputData;
	
	uint8_t m_InputData[MAX_RECV];
	uint8_t m_OutputData[MAX_SEND];

	size_t m_LastOutputSize;
	size_t m_LastInputSize;

	bool bInterrupt;
	bool bHandlerActive[4];

	handler_allocator allocator_;

	CUser *m_User;

public:
	CNet(boost::asio::io_service &service) : m_Service(service), m_Socket(service),
		m_OutputBuffer(boost::asio::streambuf(0)), m_LastOutputSize(0),
		m_LastInputSize(0), bInterrupt(false), m_User(NULL)
	{
		Initialize();
	}

	void Initialize()
	{
		memset(&bHandlerActive, 0, sizeof(bool) * 4); 
		memset(&m_OutputData, 0, MAX_SEND);
	}

	~CNet();

	boost::asio::ip::tcp::socket &GetSocket();

	void Update();
	void WaitForHandlerCompletion();
	void AddData(uint8_t *data, uint32_t size, bool sys = false);

	void Write(bool add = false);
	void WriteHandler(boost::system::error_code ec, std::size_t s);

	void Read();
	void ReadHandler(const boost::system::error_code& ec, std::size_t s);

	void Start();
	void Stop();

	void BindUser(CUser *user);

	void DeleteLater()
	{
		bInterrupt = true;
	}

	CUser *GetUser();
};

class CListener
{
private:
	boost::asio::io_service &MainService;
	boost::asio::ip::tcp::acceptor Acceptor;

	CNet *NET;
	handler_allocator m_Allocator;

public:
	CListener(boost::asio::io_service &main) : MainService(main), 
		Acceptor(main, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 2100)),
		NET(NULL)
	{
	}

	void Start();
	void HandleAccept(const boost::system::error_code &ec);
};

#endif