#include "stdafx.h"
#include "Core.h"
#include "Packets.h"
#include "Allocation.h"

CCore *sv;
CConsole *cmd;
CListener *listener;

//prevent new allocation . if single threaded

static	SNewPPacket				_AddPlayer;
static 	SNewGPacket				_AddGame;
static	SRemoveGPacket			_RemoveGame;
static	SRemovePPacket			_RemovePlayer;
static	SChatPacket				_Chat;
static	SConnectPacket			_Connect;
static	SChangeNamePacket		_ChangeName;
static	SRequestPasswordPacket	_RequestPassword;
static	SRefreshGamePacket		_RefreshGame;
static	SPlayerDataPacket		_PlayerData;
static	SGameStatusPacket		_GameStatus;
static	SGameHostPacket			_GameHost;
static	SJoinGamePacket			_JoinGame;

void ServerInit()
{
	sv = new CCore();

	memset(&_AddPlayer, 0, sizeof(SNewPPacket));
	memset(&_AddGame, 0, sizeof(SNewGPacket));
	memset(&_RemoveGame, 0, sizeof(SRemoveGPacket));
	memset(&_RemovePlayer, 0, sizeof(SRemovePPacket));
	memset(&_Connect, 0, sizeof(SConnectPacket));
	memset(&_Chat, 0, sizeof(SChatPacket));
	memset(&_ChangeName, 0, sizeof(SChangeNamePacket));
	memset(&_RequestPassword, 0, sizeof(SRequestPasswordPacket));
	memset(&_RefreshGame, 0, sizeof(SRefreshGamePacket));
	memset(&_PlayerData, 0, sizeof(SPlayerDataPacket));
	memset(&_GameStatus, 0, sizeof(SGameStatusPacket));
	memset(&_GameHost, 0, sizeof(SGameHostPacket));
	memset(&_JoinGame, 0, sizeof(SJoinGamePacket));

	_AddPlayer.header = PLAYER_ADD;
	_AddGame.header = GAME_ADD;
	_RemoveGame.header = GAME_REMOVE;
	_RemovePlayer.header = PLAYER_REMOVE;
	_Connect.header = CONNECT;
	_Chat.header = CHAT;
	_ChangeName.header = CHANGE_NAME;
	_RequestPassword.header = REQUEST_PASSWORD;
	_RefreshGame.header = GAME_REFRESH;
	_PlayerData.header = PLAYER_DATA;
	_GameStatus.header = GAME_STATUS;
	_GameHost.header = GAME_HOSTED;
	_JoinGame.header = JOIN_GAME;

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

	net->Start();

	if(strcmp(ip.c_str(), "127.0.0.1") != 0)
	{
		if(sv->IpIsBlocked(ip))
		{
			printf("Blocked IP: %s\n", ip.c_str());
			net->DeleteLater();
			net = NULL;

			StartAccept();
			return;
		}
		if(sv->MaxConnectionsReached(ip))
		{
			printf("Max connections reached.\n");
			net->DeleteLater();
			net = NULL;

			StartAccept();
			return;
		}
	}

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

std::string CheckName(const char *name)
{
	CUser *user = NULL;
	std::string res(name);
	long long count = 1;

RECHECK:
	if(count > 9)
	{
		return "";
	}
	for(unsigned int i = 0; i < sv->GetUserList()->size(); ++i)
	{
		user = sv->GetUserList()->at(i);
		if(strcmp(user->GetName().c_str(), res.c_str()) == 0)
		{
			if(res.size() > 18)
			{
				res.resize(18);
			}
			res = std::string(res + "_" + std::to_string(count));

			count++;
			goto RECHECK;
		}
	}

	return res;
}

void Analyze(uint8_t *data, uint32_t size, CNet *net)
{
	CUser *user = net->GetUser();

	if(!user)
	{
		return;
	}

	uint16_t header = *(uint16_t*)data;

	if(user)
	{
		switch(header)
		{
		case CONNECT:
			{
				if(user->GetName().size())
				{
					break;
				}

				_AddPlayer = *(SNewPPacket*)data;
				_AddPlayer.header = PLAYER_ADD;
				_AddPlayer.id = user->GetId();
				_AddPlayer.status = user->GetStatus();

				int check = sv->CheckUserStatus(std::string(_AddPlayer.Name).c_str());
				if(check == STATUS_REGISTERED)
				{
					break;
				}

				std::string str = CheckName(_AddPlayer.Name);
				bool changed = strcmp(str.c_str(), _AddPlayer.Name) != 0;

				strcpy_s(_AddPlayer.Name, str.c_str());

				if(changed)
				{
					_ChangeName.id = user->GetId();
					_ChangeName.stats = user->GetRank();

					strcpy_s(_ChangeName.name, _AddPlayer.Name);

					user->AppendData(&_ChangeName, sizeof(SChangeNamePacket));
				}

				_PlayerData.id = user->GetId();
				_PlayerData.status = user->GetStatus();

				user->AppendData(&_PlayerData, sizeof(SPlayerDataPacket));
				user->SetName(std::string(_AddPlayer.Name));
				user->SetRank(_AddPlayer.Rank);

				sv->SendToAll(&_AddPlayer, sizeof(SNewPPacket), user);

				for(unsigned int i = 0; i < sv->GetUserList()->size(); ++i)
				{
					CUser *_user = sv->GetUserList()->at(i);
					if(_user)
					{
						if(_user != user)
						{
							_AddPlayer.id = _user->GetId();
							_AddPlayer.status = _user->GetStatus();
							_AddPlayer.Rank = _user->GetRank();

							strcpy_s(_AddPlayer.Name, _user->GetName().c_str());

							user->AppendData(&_AddPlayer, sizeof(SNewPPacket));
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

						_AddGame.id = _game->GetGameID();
						_AddGame.hostId = _game->GetHostID();
						_AddGame.Players = _game->GetPlayerCount();
						_AddGame.MaxPlayers = _game->GetMaxPlayers();
						_AddGame.mapMod = _game->HasMapMod();
						_AddGame.privateGame = _game->IsPrivate();
					
						_AddGame.MinRank = _game->GetMinRank();
						_AddGame.MaxRank = _game->GetMaxRank();

						strcpy_s(_AddGame.Map, _game->GetMapName().c_str());
						strcpy_s(_AddGame.mods, _game->GetMods().c_str());

						if(_game->GetHost() != NULL)
						{
							strcpy_s(_AddGame.ip, _game->GetHost()->GetIp().c_str());
						}
						else
						{
							strcpy_s(_AddGame.ip, "127.0.0.1");
						}

						user->AppendData(&_AddGame, sizeof(SNewGPacket));
					}
				}

				break;
			}
		case HOST_GAME:
			{
				if(sv->CheckAnyHostIP(user->GetIp()))
				{
					break;
				}

				user->SetGameStatus(STATUS_PLAYING);

				_AddGame = *(SNewGPacket*)data;
				_AddGame.header = GAME_ADD;

				CGame *game = new CGame((std::string(_AddGame.Name).c_str()), (std::string(_AddGame.Map).c_str()), _AddGame.MaxPlayers, (std::string(_AddGame.Version).c_str()), 
					(std::string(_AddGame.mods).c_str()), (std::string("").c_str()), _AddGame.mapMod == 1, _AddGame.privateGame == 1);

				_AddGame.id = game->GetGameID();
				_AddGame.hostId = game->GetHostID();

				user->JoinGame(game);
				sv->AddGame(game);

				strcpy_s(_AddGame.ip, game->GetHost()->GetIp().c_str());

				_GameHost.gId = game->GetGameID();
				_GameStatus.id = user->GetId();
				_GameStatus.status = user->GetGameStatus();

				user->AppendData(&_GameHost, sizeof(SGameHostPacket));
				sv->SendToAll(&_AddGame, sizeof(SNewGPacket));
				sv->SendToAll(&_GameStatus, sizeof(SGameStatusPacket));

				break;
			}
		case JOIN_GAME:
			{
				_JoinGame = *(SJoinGamePacket*)data;

				CGame *game = sv->GetGameById(_JoinGame.gId);
				if(game != NULL)
				{
					game->AddUser(user);
				}

				user->SetGameStatus(STATUS_PLAYING);
				
				break;
			}
		case GAME_REFRESH:
			{
				_RefreshGame = *(SRefreshGamePacket*)data;

				if(user->GetGame() != NULL)
				{
					if(user->GetGame()->IsHost(user))
					{
						user->GetGame()->SetPlayerCount(_RefreshGame.players);
						user->GetGame()->SetMaxPlayerCount(_RefreshGame.maxPlayers);
					}
				}

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

				sv->SendToAll(&_Chat, sizeof(SChatPacket), user, STATUS_LOBBY);
					
				break;
			}
		case CHANGE_NAME:
			{
				_ChangeName = *(SChangeNamePacket*)data;
				_ChangeName.id = user->GetId();

				int check = sv->CheckUserStatus(_ChangeName.name);
				if(check == STATUS_REGISTERED)
				{
					break;
				}

				std::string str = CheckName(_ChangeName.name);
				bool changed = strcmp(str.c_str(), _ChangeName.name) != 0;

				strcpy_s(_ChangeName.name, str.c_str());

				if(changed)
				{
					_ChangeName.id = user->GetId();
					_ChangeName.stats = user->GetRank();

					user->AppendData(&_ChangeName, sizeof(SChangeNamePacket));
				}

				user->SetName(_ChangeName.name);
				user->SetRank(_ChangeName.stats);
				sv->SendToAll(&_ChangeName, sizeof(SChangeNamePacket));

				break;
			}
		case REJOIN_LOBBY:
			{
				user->SetGameStatus(STATUS_LOBBY);

				break;
			}
		case GAME_STATUS:
			{
				_GameStatus = *(SGameStatusPacket*)data;
				_GameStatus.id = user->GetId();

				if(_GameStatus.status == user->GetGameStatus() ||
					(_GameStatus.status != STATUS_PLAYING && _GameStatus.status != STATUS_LOBBY))
				{
					break;
				}

				user->SetGameStatus((EGameStatus)_GameStatus.status);
				sv->SendToAll(&_GameStatus, sizeof(SGameStatusPacket));

				break;
			}
		case GAME_REMOVE:
			{
				_RemoveGame = *(SRemoveGPacket*)data;

				CGame *game = sv->GetGameById(_RemoveGame.id);
				if(game != NULL)
				{
					if(game->GetHost() == user)
					{
						sv->RemoveGame(game);
					}
				}

				break;
			}
		default:
			{
				sv->BlockIp(user->GetIp());

				printf("Received invalid packet. Header: %d\n", *(uint16_t*)data);

				sv->RemoveConnection(user);
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

#ifdef UPDATE_USERS
			if(m_Allocated && GetRunTime() > m_NextRefreshTime)
			{
				sv->UpdateUser(this);

				m_NextRefreshTime = GetRunTime() + 10000;
			}
#endif
		}
	}

void CUser::AppendData(uint8_t *data, uint32_t size)
	{
		if(m_Connection != NULL)
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

bool CUser::JoinGame(CGame *game)
	{
		if(game != NULL)
		{
			if(game->AddUser(this))
			{
				m_Game = game;
				return true;
			}
		}
		m_Game = NULL;

		return false;
	}

uint16_t CUser::GetId()
	{
		return m_Id;
	}

uint32_t CUser::GetLastChatCount()
	{
		return m_ChatCount;
	}

PLAYERSTATS CUser::GetRank()
	{
		return m_Rank;
	}

void CUser::SetName(std::string name)
	{
		m_Name = name;
		m_Allocated = true;
	}

void CUser::SetRank(PLAYERSTATS rank)
	{
		m_Rank = rank;
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
		if(m_InLobby && GetRunTime() > m_NextRefreshTime)
		{
			m_NextRefreshTime = GetRunTime() + 10000;
			sv->UpdateGame(this);
		}
	}

void CGame::SetPlayerCount(uint8_t players)
	{
		m_Players = players;
	}

void CGame::SetMaxPlayerCount(uint8_t max)
	{
		m_MaxPlayers = max;
	}

CUser *CGame::GetHost()
	{
		if(m_Users.size())
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

PLAYERSTATS CGame::GetMinRank()
	{
		return m_MinRank;
	}

PLAYERSTATS CGame::GetMaxRank()
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
		//Host should be first in da vector.
		if(m_Users.size())
		{
			if(m_Users[0] == user)
			{
				return true;
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
#ifdef LOBBY_HANDLE_GAME_DATA

		SGRemovePPacket p(user->GetIGameId(), (uint8_t)reason);

		if(m_Users.size())
		{
			BOOST_FOREACH(CUser *_user, m_Users)
			{
				_user->AppendData((uint8_t*)&p, sizeof(SGRemovePPacket));
			}
		}

		m_Players--;

#endif
		user->SetGameId(-1);
		user->SetIGameId(-1);

		if(IsHost(user))
		{
			sv->RemoveGame(this);
		}
	}

bool CGame::AddUser(CUser *user)
	{
		if(m_Players + 1 > m_MaxPlayers)
		{
			return false;
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

		return true;
	}

void CGame::Close()
	{
		BOOST_FOREACH(CUser *user, m_Users)
		{
			if(user)
			{
				user->JoinGame(NULL);
			}
		}
		sv->RemoveGame(this);
	}

void CGame::StartGame()
	{
		m_InLobby = false;
	}


///////////////////////////

//		CORE

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
			if(user)
			{
				user->AppendData(&p, sizeof(SShutDownPacket));
				delete user;
			}
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
			if(user)
			{
				user->AppendData(data, size);
			}
		}
	}

void CCore::SendToAll(void *data, uint32_t size, CUser *except, uint8_t status)
	{
		BOOST_FOREACH(CUser *user, UserList)
		{
			if(user && user != except)
			{
				if(status == UINT8_MAX || user->GetGameStatus() == status)
				{
					user->AppendData(data, size);
				}
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

std::vector<CUser *> CCore::GetUsersByIP(std::string ip)
	{
		std::vector<CUser *> v;

		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(strcmp(UserList[i]->GetIp().c_str(), ip.c_str()) == 0)
			{
				v.push_back(UserList[i]);
			}
		}

		return v;
	}

std::vector<CUser *> CCore::GetUsersByName(std::string name)
	{
		std::vector<CUser *> v;

		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(strcmp(UserList[i]->GetName().c_str(), name.c_str()) == 0)
			{
				v.push_back(UserList[i]);
			}
		}

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

CGame *CCore::GetGameById(uint16_t id)
	{
		for(unsigned int i = 0; i < GameList.size(); ++i)
		{
			if(GameList[i]->GetGameID() == id)
			{
				return GameList[i];
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
			SRemovePPacket p(user->GetId());
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
			SRemovePPacket p(user->GetId());
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

		_RemoveGame.id = game->GetGameID();
		SendToAll(&_RemoveGame, sizeof(SRemoveGPacket));

		delete game;
	}

void CCore::UpdateGame(CGame *game)
	{
		SRefreshGamePacket p(game->GetGameID(), game->GetPlayerCount(), game->GetMaxPlayers());

		SendToAll(&p, sizeof(SRefreshGamePacket));
	}

void CCore::UpdateUser(CUser *user)
	{
		SRefreshPlayerPacket p(user->GetId(), user->GetGameStatus());

		SendToAll(&p, sizeof(SRefreshPlayerPacket));
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
					printf("No user online with such name.\n");
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
					printf("No user online with such name.\n");
				}
			}
		}
	}