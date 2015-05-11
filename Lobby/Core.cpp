#include "stdafx.h"
#include "Core.h"
#include "Packets.h"
#include "Allocation.h"

CCore *sv;
CConsole *cmd;
CListener *listener;

//prevent new allocation . if single threaded

static	SNewPPacket				_AddP;
static 	SNewGPacket				_AddG;
static	SRemoveGPacket			_RemoveG;
static	SRemovePPacket			_RemoveP;
static	SChatPacket				_Chat;
static	SConnectPacket			_Connect;
static	SChangeNamePacket		_ChangeName;
static	SRequestPasswordPacket	_RequestPassword;
static	SRefreshGamePacket		_RefreshGame;
static	SPlayerDataPacket		_PlayerData;
static	SGameStatusPacket		_GameStatus;
static	SGameHostPacket			_GameHost;

void ServerInit()
{
	sv = new CCore();

	memset(&_AddP, 0, sizeof(SNewPPacket));
	memset(&_AddG, 0, sizeof(SNewGPacket));
	memset(&_RemoveG, 0, sizeof(SRemoveGPacket));
	memset(&_RemoveP, 0, sizeof(SRemovePPacket));
	memset(&_Connect, 0, sizeof(SConnectPacket));
	memset(&_Chat, 0, sizeof(SChatPacket));
	memset(&_ChangeName, 0, sizeof(SChangeNamePacket));
	memset(&_RequestPassword, 0, sizeof(SRequestPasswordPacket));
	memset(&_RefreshGame, 0, sizeof(SRefreshGamePacket));
	memset(&_PlayerData, 0, sizeof(SPlayerDataPacket));
	memset(&_GameStatus, 0, sizeof(SGameStatusPacket));
	memset(&_GameHost, 0, sizeof(SGameHostPacket));

	_AddP.header = PLAYER_ADD;
	_AddG.header = GAME_ADD;
	_RemoveG.header = GAME_REMOVE;
	_RemoveP.header = PLAYER_REMOVE;
	_Connect.header = CONNECT;
	_Chat.header = CHAT;
	_ChangeName.header = CHANGE_NAME;
	_RequestPassword.header = REQUEST_PASSWORD;
	_RefreshGame.header = GAME_REFRESH;
	_PlayerData.header = PLAYER_DATA;
	_GameStatus.header = GAME_STATUS;
	_GameHost.header = GAME_HOSTED;

	cmd = new CConsole();
	cmd->Run();
}

void Stop()
{
	delete listener;
	delete sv;
	delete cmd;
}

CCore *CORE()
{
	return sv;
}

std::vector<CUser *> EnqueuedUsers;

SShutDownPacket ShutDown()
{
	return *new SShutDownPacket();
}

uint16_t GeneratePlayerID()
{
	PlayerCount++;
	return PlayerCount - 1;
}

uint16_t GenerateGameID()
{
	GameCount++;
	return (GAME_START_ID + GameCount - 1);
}

void AcceptCallback(CNet *net)
{
	std::string ip = net->GetSocket().remote_endpoint().address().to_string().c_str();

	if(sv->IpIsBlocked(ip))
	{
		printf("Blocked IP: %s\n", ip.c_str());
		delete net;
		net = NULL;
		return;
	}
	if(sv->MaxConnectionsReached(ip))
	{
		delete net;
		net = NULL;
		return;
	}

	net->Start();

	CUser *user = new CUser(net, ip);

	sv->AddUser(user);
	StartAccept();
}

void CreateListener(boost::asio::io_service &service)
{
	listener = new CListener(service);
}

void StartAccept()
{
	listener->Start();
}

void Analyze(uint8_t *data, uint32_t size, CNet *net)
{
	CUser *user = net->GetUser();

	if(!user)
	{
		return;
	}

	if(user)
	{
		switch(*(uint16_t*)data)
		{
		case CONNECT:
			{
				if(user->GetName().size())
				{
					break;
				}

				_AddP = *(SNewPPacket*)data;
				_AddP.header = PLAYER_ADD;
				_AddP.id = user->GetId();
				_AddP.status = user->GetStatus();

				int check = sv->CheckUserStatus(std::string(_AddP.Name).c_str());
				if(check == STATUS_REGISTERED)
				{
					break;
				}

				bool changed = false;
				long double count = 1;

CHECK_NAME:
				check = sv->NameAlreadyExist(_AddP.Name);
				if(check)
				{
					if(check > 18 || count > 9)
					{
						sv->RemoveConnection(user->GetConnection());
						break;
					}
					else
					{
						std::string str = std::string(_AddP.Name + std::string("_") + std::to_string(count));
						strcpy_s(_AddP.Name, str.c_str());
						count++;

						changed = true;
						goto CHECK_NAME;
					}
				}
				if(changed)
				{
					_ChangeName.id = user->GetId();
					strcpy_s(_ChangeName.name, _AddP.Name);

					user->AppendData(&_ChangeName, sizeof(SChangeNamePacket));
				}

				_PlayerData.id = user->GetId();
				_PlayerData.status = user->GetStatus();

				user->AppendData(&_PlayerData, sizeof(uint16_t)*3);
				user->SetName(std::string(_AddP.Name));
				user->SetRank(_AddP.Rank);

				sv->SendToAll(&_AddP, sizeof(SNewPPacket), user);

				for(unsigned int i = 0; i < sv->GetUserList()->size(); ++i)
				{
					CUser *_user = sv->GetUserList()->at(i);
					if(_user)
					{
						if(_user != user)
						{
							_AddP.id = _user->GetId();
							_AddP.status = _user->GetStatus();

							strcpy_s(_AddP.Name, _user->GetName().c_str());
							memcpy((void*)_AddP.Rank, (void*)_user->GetRank(), sizeof(uint32_t) * 7);

							user->AppendData(&_AddP, sizeof(SNewPPacket));
						}
					}
				}

				for(unsigned int i = 0; i < sv->GetGameList()->size(); ++i)
				{
					CGame *_game = sv->GetGameList()->at(i);
					if(_game)
					{
						if(_game->GetHost() == NULL)
						{
							continue;
						}

						_AddG.id = _game->GetGameID();
						_AddG.hostId = _game->GetHostID();
						_AddG.Players = _game->GetPlayerCount();
						_AddG.MaxPlayers = _game->GetMaxPlayers();
						_AddG.mapMod = _game->HasMapMod();
						_AddG.privateGame = _game->IsPrivate();
					
						memcpy(&_AddG.MinRank, (void*)_game->GetMinRank(), sizeof(uint32_t) * 7);
						memcpy(&_AddG.MaxRank, (void*)_game->GetMaxRank(), sizeof(uint32_t) * 7);

						strcpy_s(_AddG.Map, _game->GetMapName().c_str());
						strcpy_s(_AddG.mods, _game->GetMods().c_str());
						strcpy_s(_AddG.ip, _game->GetHost()->GetIp().c_str());

						user->AppendData(&_AddG, sizeof(SNewGPacket));
					}
				}

				_GameStatus.status = 1;
				user->AppendData(&_GameStatus, sizeof(SGameStatusPacket));

				break;
			}
		case HOST_GAME:
			{
				if(sv->CheckAnyHostIP(user->GetIp()))
				{
					break;
				}

				user->SetGameStatus(STATUS_PLAYING);

				_AddG = *(SNewGPacket*)data;
				_AddG.header = GAME_ADD;

				CGame *game = new CGame((std::string(_AddG.Name).c_str()), (std::string(_AddG.Map).c_str()), _AddG.MaxPlayers, (std::string(_AddG.Version).c_str()), 
					(std::string(_AddG.mods).c_str()), (std::string("").c_str()), _AddG.mapMod == 0, _AddG.privateGame == 0);

				_AddG.id = game->GetGameID();

				user->JoinGame(game);
				sv->AddGame(game);

				_GameHost.gId = game->GetGameID();
				user->AppendData(&_GameHost, sizeof(SGameHostPacket));

				sv->SendToAll(&_AddG, sizeof(SNewGPacket));

				break;
			}
		case CHAT:
			{
				if(!user->CanChat())
				{
					break;
				}

				user->DoChat();

				_Chat.id = user->GetId();
				strcpy_s(_Chat.Text, (*(SChatPacket*)data).Text);

				sv->SendToAll(&_Chat, sizeof(SChatPacket));
					
				break;
			}
		case CHANGE_NAME:
			{
				_ChangeName = *(SChangeNamePacket*)data;
				_ChangeName.id = user->GetId();

				int check = sv->CheckUserStatus(std::string(_ChangeName.name).c_str());
				if(check == STATUS_REGISTERED)
				{
					break;
				}

				long double count = 1;

CHECK_RENAME:
				check = sv->NameAlreadyExist(_ChangeName.name);
				if(check)
				{
					if(check > 18 || count > 9)
					{
						sv->RemoveConnection(user->GetConnection());
						break;
					}
					else
					{
						std::string str = std::string(_ChangeName.name + std::string("_") + std::to_string(count));
						strcpy_s(_ChangeName.name, str.c_str());
						count++;

						goto CHECK_RENAME;
					}
				}

				user->SetName(_ChangeName.name);
				sv->SendToAll(&_ChangeName, sizeof(SChangeNamePacket), user);

				break;
			}
		case REJOIN_LOBBY:
			{
				user->SetGameStatus(STATUS_LOBBY);

				break;
			}
		default:
			{
				sv->BlockIp(user->GetIp());

				printf("Received invalid packet. Header: %d", *(uint16_t*)data);

				delete user;
				break;
			}
		}
	}
}

/////////////////////////////

//     USER

////////////////////////////

CUser::~CUser()
	{
		if(m_Game != NULL)
		{
			if(m_Game->IsHost(this))
			{
				m_Game->Close();
			}
			else
			{
				m_Game->RemoveUser(this, CGame::EXIT);
			}
		}
		if(m_Connection != NULL)
		{
			m_Connection->DeleteLater();
			m_Connection = NULL;
		}
	}

void CUser::Update()
	{
		uint8_t *data = NULL;
		uint32_t size = 0;

		if(GetRunTime()-m_FirstChatTime > 10000)
		{
			m_ChatCount = 0;
		}

		if(m_Connection != NULL)
		{
			m_Connection->Update();
		}

		if(GetRunTime() > m_NextRefreshTime)
		{
			sv->UpdateUser(this);

			m_NextRefreshTime = GetRunTime() + 10000;
		}
	}

void CUser::AppendData(uint8_t *data, uint32_t size)
	{
		if(m_Connection)
		{
			m_Connection->AddData(data, size);
		}
	}

void CUser::AppendData(void *data, uint32_t size)
	{
		AppendData((uint8_t*)data, size);
	}

int CUser::TryJoinGame(CGame *game)
	{
		if(game->IsFull())
		{
			return GAME_FULL;
		}
		else if(game->GetGamePassword().size() > 0)
		{
			return GAME_PASSWORD_REQUIRED;
		}

		return -1;
	}

void CUser::JoinGame(CGame *game)
	{
		m_Game = game;
		m_Game->AddUser(this);
	}

uint16_t CUser::GetId()
	{
		return m_Id;
	}

uint32_t CUser::GetLastChatCount()
	{
		return m_ChatCount;
	}

uint32_t *CUser::GetRank()
	{
		return m_Rank;
	}

void CUser::SetName(std::string name)
	{
		m_Name = name;
		m_Allocated = true;
	}

void CUser::SetRank(uint32_t rank[7])
	{
		memcpy(&m_Rank, (void*)rank, sizeof(uint32_t) * 7);
	}

void CUser::SetStatus(EUserAuthority status)
	{
		m_Status = status;
	}

void CUser::SetGameStatus(EGameStatus status)
	{
		m_GameStatus = status;
	}

uint8_t CUser::GetStatus()
	{
		return m_Status;
	}

uint8_t CUser::GetGameStatus()
	{
		return m_GameStatus;
	}

std::string CUser::GetName()
	{
		return m_Name;
	}

std::string CUser::GetIp()
	{
		return m_Ip;
	}

uint16_t CUser::GetIGameId()
	{
		return m_IGameId;
	}

uint16_t CUser::GetGameId()
	{
		return m_GameId;
	}

bool CUser::Allocated()
	{
		return m_Allocated;
	}

void CUser::DoChat()
	{
		m_ChatCount++;
		if(m_ChatCount == 1)
		{
			m_FirstChatTime = GetRunTime();
		}
	}

void CUser::BlockChat(unsigned long dwTime)
	{
		if(dwTime != -1)
		{
			m_ChatBlockEnd = dwTime * 1000 + GetRunTime();
		}
		else
		{
			m_ChatBlockEnd = dwTime;
		}
	}

bool CUser::CanChat()
	{
		if(m_ChatBlockEnd == -1 || m_ChatCount > 10 ||
			m_ChatBlockEnd > GetRunTime())
		{
			return false;
		}

		return true;
	}

CGame *CUser::GetGame()
	{
		return m_Game;
	}

CNet *CUser::GetConnection()
	{
		return m_Connection;
	}

void CUser::SetConnection(CNet *conn)
	{
		m_Connection = conn;
	}

void CUser::SetGameId(uint16_t Id)
	{
		m_GameId = Id;
	}

void CUser::SetIGameId(uint16_t Id)
	{
		m_IGameId = Id;
	}

/////////////////////////////

//     GAME

////////////////////////////


CGame::~CGame()
	{
	}

void CGame::Update()
	{
		if(GetRunTime() > m_NextRefreshTime)
		{

		}
	}

CUser *CGame::GetHost()
	{
		if(m_Users.size() > 0)
		{
			return m_Users[0];
		}

		return NULL;
	}

uint16_t CGame::GetHostID()
	{
		if(m_Users.size() > 0)
		{
			return m_Users[0]->GetId();
		}
		else
		{
			return -1;
		}
	}

uint16_t CGame::GetGameID()
	{
		return m_GameId;
	}

std::string CGame::GetMapName()
	{
		return m_Map;
	}

std::string CGame::GetGamePassword()
	{
		return m_Password;
	}

std::string CGame::GetMods()
	{
		return m_Mods;
	}

uint8_t CGame::GetMaxPlayers()
	{
		return m_MaxPlayers;
	}

uint8_t CGame::GetPlayerCount()
	{
		return m_Players;
	}

uint32_t *CGame::GetMinRank()
	{
		return m_MinRank;
	}

uint32_t *CGame::GetMaxRank()
	{
		return m_MaxRank;
	}

bool CGame::IsFull()
	{
		if(m_Players >= m_MaxPlayers)
		{
			return true;
		}

		return false;
	}

uint16_t CGame::GetNewIGameID()
	{
		int start = 1;
		int count = 0;
		for(unsigned int i = 0; i < m_Users.size(); ++i)
		{
			if(m_Users[i]->GetIGameId() != start)
			{
				count++;
			}
			if(i+1 >= m_Users.size() && count != m_Players-1)
			{
				start++;
				i = 0;
			}
			else if(i+1 >= m_Users.size() && count == m_Players-1)
			{
				return start;
			}
		}

		return m_Players;
	}

bool CGame::IsHost(CUser *user)
	{
		for(unsigned int i = 0; i < m_Users.size(); ++i)
		{
			if(m_Users[i] == user)
			{
				if(m_Users[i]->GetIGameId() == 0)
				{
					return true;
				}
			}
		}

		return false;
	}

bool CGame::HasMapMod()
	{
		return m_Modded;
	}

bool CGame::IsPrivate()
	{
		return m_Private;
	}

void CGame::RemoveUser(CUser *user, CGame::ERemoveReason reason)
	{
		SGRemovePPacket p;
		p.header = GAME_PLAYER_REMOVE;
		p.id = user->GetIGameId();
		p.reason = (uint8_t)reason;

		BOOST_FOREACH(CUser *_user, m_Users)
		{
			_user->AppendData((uint8_t*)&p, sizeof(SGRemovePPacket));
		}

		user->SetGameId(-1);
		user->SetIGameId(-1);
		m_Players--;
	}

void CGame::AddUser(CUser *user)
	{
		if(m_Players + 1 > m_MaxPlayers)
		{
			return;
		}

		m_Users.push_back(user);

		m_Players++;
		user->SetGameId(GetGameID());
		if(m_Players > 1)
		{
			user->SetIGameId(GetNewIGameID());
		}
		else
		{
			user->SetIGameId(0);
		}
	}

void CGame::Close()
	{
		sv->RemoveGame(this);
	}


///////////////////////////

// CORE

//////////////////////////


boost::asio::io_service &CCore::GetService()
	{
		return m_Service;
	}

void CCore::Run()
	{
		Update();
		StartAccept();
		m_Service.run();
	}

void CCore::AddUser(CUser *user)
	{
		UserList.push_back(user);
	}

void CCore::AddGame(CGame *game)
	{
		GameList.push_back(game);
	}

void CCore::Update()
	{
		std::for_each(UserList.begin(), UserList.end(), boost::bind(&CUser::Update, _1));
		std::for_each(GameList.begin(), GameList.end(), boost::bind(&CGame::Update, _1));

		m_Service.post(boost::bind(&CCore::Update, this));

		cmd->Get();
	}

CCore::~CCore()
	{
		SShutDownPacket p = ShutDown();
		BOOST_FOREACH(CUser *user, UserList)
		{
			user->AppendData(&p, sizeof(SShutDownPacket));
			delete user;
		}
	}

int CCore::CheckUserStatus(const char *name)
	{
		for(unsigned int i = 0; i < PlayerList.size(); ++i)
		{
			if(strcmp(PlayerList[i].c_str(), name) == 0)
			{
				return STATUS_REGISTERED;
			}
		}

		for(unsigned int i = 0; i < AdminList.size(); ++i)
		{
			if(strcmp(AdminList[i].c_str(), name) == 0)
			{
				return STATUS_ADMIN;
			}
		}

		return STATUS_PLAYER;
	}

void CCore::LoadReservedNames()
	{
	}

void CCore::LoadAdmins()
	{
	}

void CCore::BanGameFromLobby(CGame *game)
	{
	}

void CCore::SendTo(CUser *user, uint8_t *data, uint32_t size)
	{
		user->AppendData(data, size);
	}

void CCore::SendTo(CUser *user, void *data, uint32_t size)
	{
		user->AppendData(data, size);
	}

void CCore::SendToAll(uint8_t *data, uint32_t size)
	{
		BOOST_FOREACH(CUser *user, UserList)
		{
			user->AppendData(data, size);
		}
	}

void CCore::SendToAll(void *data, uint32_t size, CUser *except)
	{
		BOOST_FOREACH(CUser *user, UserList)
		{
			if(user && user != except)
			{
				user->AppendData(data, size);
			}
		}
	}

void CCore::BlockIp(std::string ip)
	{
		BlockList.push_back(ip);
	}

void CCore::UnblockIp(std::string ip)
	{
		for(unsigned int i = 0; i < BlockList.size(); ++i)
		{
			if(strcmp(BlockList[i].c_str(), ip.c_str()) == 0)
			{
				BlockList.erase(BlockList.begin() + i);
			}
		}
	}

bool CCore::IpIsBlocked(std::string ip)
	{
		for(unsigned int i = 0; i < BlockList.size(); ++i)
		{
			if(strcmp(BlockList[i].c_str(), ip.c_str()) == 0)
			{
				return true;
			}
		}

		return false;
	}

bool CCore::CheckAnyHostIP(std::string ip)
	{
		for(unsigned int i = 0; i < GameList.size(); ++i)
		{
			if(strcmp(GameList[i]->GetHost()->GetIp().c_str(), ip.c_str()) == 0)
			{
				return true;
			}
		}

		return false;
	}

bool CCore::MaxConnectionsReached(std::string ip)
	{
		int count = 0;
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(strcmp(UserList[i]->GetIp().c_str(), ip.c_str()) == 0)
			{
				count++;

				if(count > MAX_PLAYER_PER_IP)
				{
					return true;
				}
			}
		}

		return false;
	}

int CCore::NameAlreadyExist(std::string name)
	{
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(strcmp(UserList[i]->GetName().c_str(), name.c_str()) == 0)
			{
				return name.length();
			}
		}

		return 0;
	}

std::vector<CUser *> CCore::GetUsersByIP(std::string ip) //
	{
		std::vector<CUser *> v;
		return v;
	}

std::vector<CUser *> CCore::GetUsersByName(std::string name) //
	{
		std::vector<CUser *> v;
		return v;
	}

std::deque<CUser *> *CCore::GetUserList()
	{
		return &UserList;
	}

std::deque<CGame *> *CCore::GetGameList()
	{
		return &GameList;
	}

CUser *CCore::GetUserByName(const char *name)
	{
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(strcmp(UserList[i]->GetName().c_str(), name) == 0)
			{
				return UserList[i];
			}
		}

		return NULL;
	}

CUser *CCore::GetUserByNet(CNet *net)
	{
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(UserList[i]->GetConnection() == net)
			{
				return UserList[i];
			}
		}

		return NULL;
	}

CUser *CCore::GetUserById(uint16_t id)
	{
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(UserList[i]->GetId() == id)
			{
				return UserList[i];
			}
		}

		return NULL;
	}

void CCore::RemoveConnection(CUser *user)
	{
		if(!user)
		{
			return;
		}
		if(user->Allocated())
		{
			SRemovePPacket p;
			p.header = PLAYER_REMOVE;
			p.id = user->GetId();

			SendToAll(&p, sizeof(SRemovePPacket));
		}
		std::deque<CUser *>::iterator it = std::find(UserList.begin(), UserList.end(), user);
		if(it != UserList.end())
		{
			UserList.erase(it);
		}
		delete user;
	}

void CCore::RemoveConnection(CNet *connection)
	{
		CUser *user = NULL;
		int pos = 0;

		if(!connection)
		{
			return;
		}

		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(UserList[i]->GetConnection() == connection)
			{
				pos = i;
				user = UserList[i];

				break;
			}
		}

		if(user != NULL)
		{
			SRemovePPacket p;
			p.header = PLAYER_REMOVE;
			p.id = user->GetId();

			SendToAll(&p, sizeof(SRemovePPacket));

			UserList.erase(UserList.begin() + pos);
			delete user;
		}
		else
		{
			connection->Stop();
		}
	}

void CCore::RemoveGame(CGame *game)
	{
		std::deque<CGame *>::iterator it = std::find(GameList.begin(), GameList.end(), game);
		if(it == GameList.end())
		{
			return;
		}

		GameList.erase(it);

		SRemoveGPacket p;
		p.header = GAME_REMOVE;
		p.id = game->GetGameID();

		SendToAll((uint8_t*)&p, sizeof(SRemoveGPacket));
		delete game;
	}

void CCore::UpdateGame(CGame *game)
	{
		SRefreshGamePacket p;
		p.header = GAME_REFRESH;
		p.gId = game->GetGameID();
		p.players = game->GetPlayerCount();

		SendToAll((uint8_t*)&p, sizeof(SRefreshGamePacket));
	}

void CCore::UpdateUser(CUser *user)
	{
		SRefreshPlayerPacket p;
		p.header = PLAYER_REFRESH;
		p.id = user->GetId();
		p.playing = user->GetGameStatus();

		SendToAll((uint8_t*)&p, sizeof(SRefreshPlayerPacket));
	}

void CCore::Command(std::string cmd)
	{
		std::vector<std::string> split;
		boost::split(split, cmd, boost::is_any_of(std::string(" ")));

		if(!split.empty())
		{
			if(split[0] == "Kick" && split.size() > 1)
			{
				CUser *user = GetUserByName(split[1].c_str());
				if(user != NULL)
				{
					printf("Kicked user with id %d\n", user->GetId());
					RemoveConnection(user);
				}
				else
				{
					printf("No user online with such name\n");
				}
			}
			else if(split[0] == "Chatblock" && split.size() > 1)
			{
				CUser *user = GetUserByName(split[1].c_str());
				if(user != NULL)
				{
					if(split.size() == 2)
					{
						user->BlockChat();
					}
					else
					{
						int t = atoi(split[2].c_str());
						user->BlockChat(t);
					}
				}
				else
				{
					printf("No user online with such name\n");
				}
			}
		}
	}