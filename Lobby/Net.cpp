#include "stdafx.h"
#include "Net.h"
#include "Core.h"

CNet::~CNet()
	{
		if(m_User)
		{
			m_User->SetConnection(NULL);
		}
	}

boost::asio::ip::tcp::socket &CNet::GetSocket()
	{
		return m_Socket;
	}

void CNet::WaitForHandlerCompletion()
	{
		int count = 0;
		if(bInterrupt)
		{
			for(int i = 0; i < 4; ++i)
			{
				if(!bHandlerActive[i])
				{
					count++;
				}
			}
		}
		if(count == 4)
		{
			allocator_.deallocate(this);
		}
		else
		{
			m_Service.post(make_custom_alloc_handler(allocator_, boost::bind(&CNet::WaitForHandlerCompletion, this)));
		}
	}

void CNet::Update()
	{
	}

void Delete(CNet *net)
{
	delete net;
}

void CNet::AddData(uint8_t *data, uint32_t size, bool sys)
	{
		if(bInterrupt)
		{
			bHandlerActive[0] = false;
			return;
		}

		if(m_OutputData)
		{
			if(m_OutputData[0] == 0)
			{
				m_LastOutputSize = size;
				memcpy(m_OutputData, data, size);
				bHandlerActive[0] = false;
			}
			else
			{
				bHandlerActive[0] = true;
				m_Service.post(boost::bind(&CNet::AddData, this, data, size, true));
			}
		}
	}

void CNet::Write(bool sys)
	{
		if(bInterrupt)
		{
			bHandlerActive[1] = false;
			return;
		}

		if(m_OutputData[0] && m_LastOutputSize)
		{
			boost::asio::async_write(m_Socket, boost::asio::buffer(m_OutputData, m_LastOutputSize), 
				make_custom_alloc_handler(allocator_, 
				boost::bind(&CNet::WriteHandler, this, boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)));

			bHandlerActive[1] = false;
		}
		else
		{
			bHandlerActive[1] = true;
			bHandlerActive[2] = false;
			m_Service.post(boost::bind(&CNet::Write, this, true));
		}
	}

void CNet::WriteHandler(boost::system::error_code ec, std::size_t s)
	{
		int count = 0;
		if(bInterrupt)
		{
			bHandlerActive[2] = false;
			return;
		}
		if(ec)
		{
			bHandlerActive[2] = false;
			printf("Error when writing data. %s\n", ec.message().c_str());
			Stop();
			return;
		}
		else
		{
			printf("Sent data. %d\n", s);
		}

		std::fill(m_OutputData, m_OutputData + m_LastOutputSize, 0);

		m_LastOutputSize = 0;
		//_custom_alloc_handler<boost::_bi::bind_t<void, boost::_mfi::mf0<void,CNet>, boost::_bi::list1<boost::_bi::value<CNet *>>>> handler(allocator_, boost::bind(&CNet::Write, this)); //hm..
		//handler(&CNet::Write, this);

		bHandlerActive[2] = true;
		Write();
	}

void CNet::Read()
	{
		m_Socket.async_read_some(boost::asio::buffer(m_InputData), make_custom_alloc_handler(allocator_, 
			boost::bind(&CNet::ReadHandler, this, 
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)));
	}

void CNet::ReadHandler(const boost::system::error_code &ec, std::size_t s)
	{
		if(bInterrupt)
		{
			bHandlerActive[3] = false;
			return;
		}
		if(s > 1024)
		{
			printf("Received more than allowed data.\n");
			Stop();
			return;
		}
		if(ec)
		{
			if(bHandlerActive[3])
			{
				bHandlerActive[3] = false;
				printf("Error when receiving data. %s\n", ec.message().c_str());
				Stop();
				return;
			}
		}
		else
		{
			if(s >= sizeof(uint16_t))
			{
				printf("Received data. %d\n", s);
				Analyze(m_InputData, s, this);
			}
		}

		m_LastInputSize = s;
		bHandlerActive[3] = true;

		Read();
	}

void CNet::Start()
	{
		Read();
		Write();
	}
	
void CNet::Stop()
	{
		printf("Closing connection: %s\n", m_Socket.remote_endpoint().address().to_string().c_str());

		m_Socket.close();
		CORE()->RemoveConnection(this);

		WaitForHandlerCompletion();
	}

void CNet::BindUser(CUser *user)
	{
		m_User = user;
	}

CUser *CNet::GetUser()
	{
		return m_User;
	}


/////////////////////////////////////////////////////


void CListener::Start()
	{
		NET = new CNet(MainService);
		Acceptor.async_accept(NET->GetSocket(), make_custom_alloc_handler(m_Allocator, boost::bind(&CListener::HandleAccept, 
			this, boost::asio::placeholders::error)));
	}

void CListener::HandleAccept(const boost::system::error_code &ec)
	{
		if(!ec)
		{
			printf("New Connection from: %s\n", NET->GetSocket().remote_endpoint().address().to_string().c_str());

			AcceptCallback(NET);
		}
		else
		{
			printf("Error when accepting connection. %s\n", ec.message().c_str());
		}
	}
