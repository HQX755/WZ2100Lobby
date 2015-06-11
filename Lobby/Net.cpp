#include "stdafx.h"
#include "Net.h"
#include "Core.h"

CNet::~CNet()
	{
		//Recheck if all is done
		if(m_User != NULL)
		{
			m_User->SetConnection(NULL);
		}
		if(m_Socket.is_open())
		{
			m_Socket.close();
		}
	}

boost::asio::ip::tcp::socket &CNet::GetSocket()
	{
		return m_Socket;
	}

//Wait for all active handler to finish
void CNet::WaitForHandlerCompletion()
	{
		unsigned int count = 0;
		if(bInterrupt)
		{
			count = std::count(bHandlerActive, std::end(bHandlerActive), true);
		}
		//Every handler finished, can deallocate everything now
		if(count == HANDLER_NUM)
		{
			delete this;
		}
		else
		{
			m_Service.post(make_custom_alloc_handler(allocator_, boost::bind(&CNet::WaitForHandlerCompletion, this)));
		}
	}

void CNet::Update()
	{
		uint8_t *pkData;
		uint32_t size;

		//If queue empty check for additional data
		if(m_OutputQueue.empty())
		{
			if(m_User != NULL)
			{
				if(m_User->GetQueueData() != NULL)
				{
					pkData	= m_User->GetQueueData()->first;
					size	= m_User->GetQueueData()->second;

					AddData(pkData, size, true);
					m_User->QueuePopFront();
				}
			}
		}
	}

void CNet::AddData(uint8_t *data, uint32_t size, bool add, bool allocated)
	{
		//About to deallocate everything
		if(bInterrupt)
		{
			return;
		}

		if(add)
		{
			m_Strand.post(make_custom_alloc_handler(allocator_, boost::bind(&CNet::DoWrite, this, data, size, add, allocated)));
		}
		else
		{
			DoWrite(data, size, add, allocated);
		}
	}

void CNet::DoWrite(uint8_t *data, uint32_t size, bool add, bool allocated)
	{
		//About to deallocate everything
		if(bInterrupt)
		{
			return;
		}

		//Enqueue the data and start write if it's the only one to send
		m_OutputQueue.push_back(std::make_pair(data, size));

		if(m_OutputQueue.size() > 1)
		{
			return;
		}
		else
		{
			Write(add, allocated);
		}
	}

void CNet::Write(bool add, bool allocated)
	{
		//About to deallocate everything
		if(bInterrupt)
		{
			return;
		}
		//No data available
		if(m_OutputQueue.empty())
		{
			return;
		}

		uint8_t *data = m_OutputQueue.front().first;
		uint32_t size = m_OutputQueue.front().second;

		if(HEADER_OF(data) != HEADER_NONE && size != 0)
		{
			bHandlerActive[WRITE_HANDLER] = true;

			//Wait for the write callback
			boost::asio::async_write(m_Socket, boost::asio::buffer(data, size), 
				make_custom_alloc_handler(allocator_, 
				boost::bind(&CNet::WriteHandler, this, boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred,
				allocated)));
		}
		else
		{
			m_OutputQueue.pop_front();
		}
	}

void CNet::WriteHandler(boost::system::error_code ec, std::size_t s, bool allocated)
	{
		//About to deallocate everything
		if(bInterrupt)
		{
			if(allocated)
			{
				delete [m_OutputQueue.front().second] m_OutputQueue.front().first;
			}

			m_OutputQueue.clear();
			bHandlerActive[WRITE_HANDLER] = false;
			return;
		}
		//Some error occured
		if(ec)
		{
			bHandlerActive[WRITE_HANDLER] = false;
			printf("Error when writing data. %s\n", ec.message().c_str());

			if(allocated)
			{
				delete [m_OutputQueue.front().second] m_OutputQueue.front().first;
			}

			m_OutputQueue.clear();
			Stop();
			return;
		}
		//Success in sending the data
		else
		{
			printf("Sent data. %d, header: %d\n", s, HEADER_OF(m_OutputQueue[0].first));

			if(HEADER_OF(m_OutputQueue[0].first) == PLAYER_REMOVE)
			m_PacketsSent++;
		}

		m_LastOutputSize = s;

		if(allocated)
		{
			delete [m_OutputQueue.front().second] m_OutputQueue.front().first;
		}

		//Dequeue the data which was sent.
		m_OutputQueue.pop_front();

		//Check if there is still something in the queue
		if(!m_OutputQueue.empty())
		{
			Write(true, true);
		}
		//Check if any additional data have to be sent
		else if(m_User != NULL && m_User->GetQueueData() != NULL)
		{
			uint8_t *pkData = m_User->GetQueueData()->first;
			uint32_t size = m_User->GetQueueData()->second;

			AddData(pkData, size, true);
			m_User->QueuePopFront();
		}

		bHandlerActive[WRITE_HANDLER] = false;
	}

void CNet::Read()
	{
		bHandlerActive[READ_HANDLER] = true;

		m_Socket.async_read_some(boost::asio::buffer(m_InputData), make_custom_alloc_handler(allocator_, 
			boost::bind(&CNet::ReadHandler, this, 
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred)));
	}

void CNet::ReadHandler(const boost::system::error_code &ec, std::size_t s)
	{
		//About to deallocate everything
		if(bInterrupt)
		{
			bHandlerActive[READ_HANDLER] = false;
			return;
		}
		if(s > MAX_RECV)
		{
			printf("Received more data than allowed.\n");

			Stop();
			return;
		}
		//An error occured
		if(ec)
		{
			if(bHandlerActive[READ_HANDLER])
			{
				bHandlerActive[READ_HANDLER] = false;
				printf("Error when receiving data. %s\n", ec.message().c_str());

				Stop();
				return;
			}
		}
		else
		{
			//if size less than the size of an header, do not accept
			if(s >= sizeof(uint16_t))
			{
				printf("Received data. %d\n", s);
				Analyze(m_InputData, s, this);
			}
			else
			{
				Stop();
			}

			m_PacketsReceived++;
		}

		m_LastInputSize = s;
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

		if(m_User != NULL)
		{
			CORE()->RemoveConnection(this);
		}

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


void CListener::Initialize()
	{
		boost::asio::socket_base::reuse_address option(true);
		Acceptor.set_option(option);
	}

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

			delete NET;
			NET = NULL;

			Start();
		}
	}
