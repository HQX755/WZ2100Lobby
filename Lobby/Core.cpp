#include "stdafx.h"
#include "Core.h"
#include "Packets.h"
#include "Allocation.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>

CCore *sv;

//prevent new allocation . if single threaded

char ServerMessage[128];

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
static	SPlayerListPacket		_PlayerList;
static	SGameListPacket			_GameList;

void ServerInit()
{
	sv = &CCore::Instance();

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
	memset(&_PlayerList, 0, sizeof(SPlayerListPacket));
	memset(&_GameList, 0, sizeof(SGameListPacket));

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
	_PlayerList.header = PLAYER_LIST;
	_GameList.header = GAME_LIST;

	memset(ServerMessage, 0, 128);

	CConsole::Instance().Run();
}

void Stop()
{
}

CCore *CORE()
{
	return sv;
}

uint16_t PlayerID = 0;

//Need algorithm here or better an higher data type range
uint16_t GeneratePlayerID()
{
	return PlayerID++;
}

uint16_t GameID = 0;

//Need algorithm here or better an type with an bigger range
uint16_t GenerateGameID()
{
	return GameID++;
}

void AcceptCallback(CNet *net)
{
	std::string ip = net->GetSocket().remote_endpoint().address().to_string().c_str();

	net->Start();

	//Only check for blocks if it isn't the localhost ip
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
			printf("Max connections reached for ip %s.\n", ip.c_str());
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

void StartAccept()
{
	CListener::Instance().Start();
}

CGame *CreateGame(SNewGPacket *p)
{
	CGame *skGame = new CGame(
		c_string(p->Name),
		c_string(p->Map),
		p->MaxPlayers,
		c_string(""),
		c_string(""),
		c_string(""),
		p->mapMod == 1,
		p->privateGame == 1,
		p->rank > 3 ? 3 : p->rank
		);
	
	return skGame;
}

std::string CheckName(CUser *user, const char *name, uint8_t count = 1)
{
	CUser *skUser = NULL;
	std::string res(name);

	//Shouldn't add more than 1 digit
	if(count > 9)
	{
		return res;
	}

	for(unsigned int i = 0; i < sv->GetUserList()->size(); ++i)
	{
		skUser = sv->GetUserList()->at(i);
		if(skUser == user)
		{
			continue;
		}
		if(strcmp(skUser->GetName().c_str(), res.c_str()) == 0)
		{
			if(res.size() > MAX_PLAYER_NAME_LENGTH - 2)
			{
				res.resize(MAX_PLAYER_NAME_LENGTH - 2);
			}

			res = std::string(res + "_" + std::to_string((uint64_t)count));

			//Recheck
			return CheckName(user, res.c_str(), count++);
		}
	}

	return res;
}

//Sends all games at once
void SendGameList(CUser *user, bool filtered = true)
{
	if(sv->GetGameList()->empty())
	{
		return;
	}

	//Create an new buffer for the real gamelist
	char *c_szGameList = new char[sv->GetGameList()->size() * sizeof(SNewGPacket)];

	SNewGPacket kNewGamePacket;
	SGameListPacket kGameListPacket;
	kGameListPacket.games = sv->GetGameList()->size();
	kGameListPacket.id = 0;

	//Send the information packet
	user->Send(&kGameListPacket, sizeof(SGameListPacket));

	for(unsigned i = 0; i < sv->GetGameList()->size(); ++i)
	{
		CGame *skGame = sv->GetGameList()->at(i);
		if(skGame != NULL)
		{
			//Some checks. The display of versions shouldn't be important, since two different versions don't fit.
			if(skGame->GetHost() == NULL || (skGame->GetHost() != NULL && filtered && skGame->GetHost()->GetVersion() != user->GetVersion()))
			{
				if(i + 1 >= sv->GetGameList()->size())
				{
					//Send the real gamelist
					user->SendQueue(c_szGameList, (i) * sizeof(SNewGPacket));
				}

				continue;
			}

			//Serialize a new game packet and copy it to the real list
			kNewGamePacket.id			= skGame->GetGameID();
			kNewGamePacket.hostId		= skGame->GetHostID();
			kNewGamePacket.Players		= skGame->GetPlayerCount();
			kNewGamePacket.MaxPlayers	= skGame->GetMaxPlayers();
			kNewGamePacket.mapMod		= skGame->HasMapMod();
			kNewGamePacket.privateGame	= skGame->IsPrivate();
			kNewGamePacket.rank			= skGame->GetRankLevel();

			sstrcpy(kNewGamePacket.Map, skGame->GetMapName().c_str());
			sstrcpy(kNewGamePacket.Name, skGame->GetGameName().c_str());

			if(skGame->GetHost() != NULL)
			{
				sstrcpy(kNewGamePacket.ip, skGame->GetHost()->GetIp().c_str());
			}
			else
			{
				sstrcpy(kNewGamePacket.ip, "127.0.0.1");
			}

			memcpy(c_szGameList + sizeof(SNewGPacket) * i, &kNewGamePacket, sizeof(SNewGPacket));
		}

		if(i + 1 >= sv->GetGameList()->size())
		{
			//Send the real gamelist
			user->SendQueue(c_szGameList, sizeof(SNewGPacket) * (i + 1));
		}
	}

	if(c_szGameList)
	{
		delete [] c_szGameList;
	}
}

void SendPlayerList(CUser *user, bool all_at_once = false, unsigned int players_per_list = 10)
{
	char *c_szPlayerList;
	if(sv->GetUserList()->size() < 2)
	{
		return;
	}

	SNewPPacket	kNewPlayerPacket;
	SPlayerListPacket kPlayerList;

	//Send all at once
	if(all_at_once)
	{
		c_szPlayerList = new char[(sv->GetUserList()->size() - 1) * sizeof(SNewPPacket)];
		for(unsigned int i = 0; i < sv->GetUserList()->size(); ++i)
		{
			CUser *skUser = sv->GetUserList()->at(i);
			if(skUser != user)
			{
				kNewPlayerPacket.id = skUser->GetId();
				kNewPlayerPacket.playing = skUser->GetGameStatus();
				kNewPlayerPacket.Rank = skUser->GetRank();
				kNewPlayerPacket.status = skUser->GetStatus();

				sstrcpy(kNewPlayerPacket.Name, skUser->GetName().c_str());
				mmemcpy(c_szPlayerList + sizeof(SNewPPacket) * i, &kNewPlayerPacket, sizeof(SNewPPacket));
			}
		}
		
		user->Send(c_szPlayerList, (sv->GetUserList()->size() - 1) * sizeof(SNewPPacket));
	}
	else
	{
		//Allocate new list to send more players at once
		c_szPlayerList = new char[min(players_per_list, sv->GetUserList()->size()) * sizeof(SNewPPacket)];

		kPlayerList.players = min(players_per_list, sv->GetUserList()->size());
		kPlayerList.id		= 0;

		unsigned iPlayers = kPlayerList.players;

		for(unsigned j = 0; j < sv->GetUserList()->size(); j += players_per_list)
		{
			kPlayerList.players = iPlayers = min(players_per_list, sv->GetUserList()->size() - kPlayerList.id * players_per_list);

			for(unsigned i = 0; i < iPlayers; ++i)
			{
				CUser *skUser = sv->GetUserList()->at(j+i);
				if(skUser != NULL)
				{
					if(skUser != user)
					{
						kNewPlayerPacket.id		= skUser->GetId();
						kNewPlayerPacket.status	= skUser->GetStatus();
						kNewPlayerPacket.Rank	= skUser->GetRank();

						sstrcpy(kNewPlayerPacket.Name, skUser->GetName().c_str());

						if((i + 1) % players_per_list == 0)
						{
							user->Send(&kPlayerList, sizeof(SPlayerListPacket));
							user->SendQueue(c_szPlayerList, sizeof(SNewPPacket) * 10);
						}

						uint8_t *pNewPlayerPacket = (uint8_t*)&kNewPlayerPacket;
						mmemcpy(c_szPlayerList + i * sizeof(SNewPPacket), pNewPlayerPacket, sizeof(SNewPPacket));
					}
					else
					{
						kPlayerList.players--;
					}
					if(j + i + 1 >= sv->GetUserList()->size() && ((kPlayerList.players > 0 || user != skUser)) && (i == 0 || (i + 1) % players_per_list != 0))
					{
						user->Send(&kPlayerList, sizeof(SPlayerListPacket));
						user->SendQueue(c_szPlayerList, sizeof(SNewPPacket) * i);
					}
				}
			}

			kPlayerList.id++;
		}
	}

	if(c_szPlayerList)
	{
		delete [] c_szPlayerList;
	}
}

void SendChangeName(CUser *user, void *data, bool all = false)
{
	SChangeNamePacket kChangeNamePacket = *(SChangeNamePacket*)data;

	if(all)
	{
		sv->SendToAll(&kChangeNamePacket, sizeof(SChangeNamePacket));
	}
	else
	{
		kChangeNamePacket.id = 0;
		user->Send(&kChangeNamePacket, sizeof(SChangeNamePacket));
	}
}

void RecvChangeName(CUser *user, void *data)
{
	SChangeNamePacket kChangeNamePacket = *(SChangeNamePacket*)data;

	//Check if registered, cancel if so and wait for a password response.
	int iCheck = sv->CheckUserStatus(std::string(_AddPlayer.Name).c_str());
	if(iCheck == STATUS_REGISTERED)
	{
		user->Send(&_RequestPassword, sizeof(uint16_t));
		return;
	}

	//Name stayed the same
	if(strcmp(kChangeNamePacket.name, user->GetName().c_str()) == 0)
	{
		return;
	}
	
	//Check if name already exist
	std::string skNewName = CheckName(user, kChangeNamePacket.name);
	if(strcmp(skNewName.c_str(), kChangeNamePacket.name) != 0)
	{
		sstrcpy(kChangeNamePacket.name, skNewName.c_str());
	}

	SendChangeName(user, &kChangeNamePacket, true);
}

void ParseConnect(CUser *user, void *data)
{
	SConnectPacket kConnectPacket = *(SConnectPacket*)data;

	//Check if registered, cancel if so and wait for a password response.
	int iCheck = sv->CheckUserStatus(std::string(_AddPlayer.Name).c_str());
	if(iCheck == STATUS_REGISTERED)
	{
		user->Send(&_RequestPassword, sizeof(uint16_t));
		return;
	}
	
	//Check if name already exist
	std::string skNewName = CheckName(user, kConnectPacket.Name);
	if(strcmp(skNewName.c_str(), kConnectPacket.Name) != 0)
	{
		sstrcpy(kConnectPacket.Name, skNewName.c_str());
		
		SChangeNamePacket kChangeNamePacket;
		kChangeNamePacket.id = user->GetId();
		sstrcpy(kChangeNamePacket.name, skNewName.c_str());

		SendChangeName(user, &kChangeNamePacket);
	}

	user->SetName(skNewName);
	user->SetRank(kConnectPacket.Rank);

	//Notify the user about his id and status
	SPlayerDataPacket kPlayerData;
	kPlayerData.id		= user->GetId();
	kPlayerData.status	= user->GetStatus();

	user->Send(&kPlayerData, sizeof(SPlayerDataPacket));

	//Notify everyone about the new player
	SNewPPacket kNewPlayerPacket;
	kNewPlayerPacket.Rank		= user->GetRank();
	kNewPlayerPacket.id			= user->GetId();
	kNewPlayerPacket.status		= user->GetStatus();
	kNewPlayerPacket.playing	= user->GetGameStatus();

	sstrcpy(kNewPlayerPacket.Name, user->GetName().c_str());

	sv->SendToAll(&kNewPlayerPacket, sizeof(SNewPPacket), user, UINT8_MAX, &user->GetVersion());

	//Send all games and players to the newcomer
	SendPlayerList(user);
	SendGameList(user, false);
}

void SendChat(CUser *user, void *data, uint32_t size)
{
	boost::shared_ptr<char> pkChat(new char[size]);

	SChatPacket kChatPacket = *(SChatPacket*)data;
	kChatPacket.id = user->GetId();

	mmemcpy(pkChat.get(), &kChatPacket, sizeof(SChatPacket));
	mmemcpy(pkChat.get() + sizeof(SChatPacket), (uint8_t*)data + sizeof(SChatPacket), size - sizeof(SChatPacket));

	//Send to a specific player
	if(kChatPacket.to)
	{
		CUser *to = sv->GetUserById(kChatPacket.to);
		if(to != NULL)
		{
			to->Send(pkChat.get(), size);
		}
	}
	//Send to everyone who is currently in lobby
	else
	{
		sv->SendToAll(pkChat.get(), size, NULL, STATUS_LOBBY);
	}
}

void SendHostGame(CUser *user, void *data)
{
	SNewGPacket kNewGamePacket = *(SNewGPacket*)data;

	//Try to create new game
	CGame *skGame = CreateGame(&kNewGamePacket);
	if(skGame == NULL)
	{
		return;
	}

	//Add the game to the list and set the player as host
	user->JoinGame(skGame);
	sv->AddGame(skGame);

	//Send the game to the players
	kNewGamePacket.header	= GAME_ADD;
	kNewGamePacket.id		= skGame->GetGameID();
	kNewGamePacket.hostId	= user->GetId();
	sstrcpy(kNewGamePacket.ip, user->GetIp().c_str());
	
	sv->SendToAll(&kNewGamePacket, sizeof(SNewGPacket), user, STATUS_LOBBY);

	//Notify the user that the game is hosted now
	SGameHostPacket kHostPacket;
	kHostPacket.gId = skGame->GetGameID();

	char *c_szHost			= new char[sizeof(SGameHostPacket) + strlen(ServerMessage)];
	uint8_t *pHostPacket	= (uint8_t*)&kHostPacket;

	mmemcpy(c_szHost, pHostPacket, sizeof(SGameHostPacket));
	mmemcpy(c_szHost + sizeof(SGameHostPacket), ServerMessage, strlen(ServerMessage));

	user->Send(c_szHost, sizeof(SGameHostPacket) + strlen(ServerMessage));

	//Update the user status, he can't be in lobby at this moment
	SGameStatusPacket kGameStatus;
	kGameStatus.id		= user->GetId();
	kGameStatus.status	= user->GetGameStatus();

	sv->SendToAll(&kGameStatus, sizeof(SGameStatusPacket), user);
}

//Send a audio signal
void SendSignal(CUser *user, void *data)
{
	SSignalPacket kSignalPacket = *(SSignalPacket*)data;

	CUser *skUser = sv->GetUserById(kSignalPacket.to);
	if(skUser == NULL)
	{
		return;
	}
	else
	{
		skUser->Send(&kSignalPacket, sizeof(SSignalPacket));
	}
}

void RecvJoinGame(CUser *user, void *data)
{
	SJoinGamePacket kJoinGame = *(SJoinGamePacket*)data;
	
	//Check if game exist
	CGame *skGame = sv->GetGameById(kJoinGame.gId);
	if(skGame == NULL)
	{
		return;
	}
	else
	{
		skGame->AddUser(user);
	}

	user->SetGameStatus(STATUS_PLAYING);
}

void RecvRefreshGame(CUser *user, void *data)
{
	SRefreshGamePacket kRefreshGame = *(SRefreshGamePacket*)data;

	//Check if game exist
	CGame *skGame = sv->GetGameById(kRefreshGame.gId);
	if(skGame == NULL)
	{
		return;
	}
	else
	{
		//Check if this user is the game's host
		if(skGame->GetHost() == user)
		{
			skGame->SetPlayerCount(kRefreshGame.players);
			skGame->SetMaxPlayerCount(kRefreshGame.maxPlayers);
		}
	}
}

void SendPlayerGameStatus(CUser *user, void *data)
{
	SGameStatusPacket kGameStatus = *(SGameStatusPacket*)data;

	//Check if valid status
	if(kGameStatus.status == user->GetGameStatus() ||
		(kGameStatus.status != STATUS_LOBBY && kGameStatus.status != STATUS_PLAYING))
	{
		return;
	}
	else
	{
		user->SetGameStatus((EGameStatus)kGameStatus.status);
		sv->SendToAll(&kGameStatus, sizeof(SGameStatusPacket), user);
	}
}

void RecvRemoveGame(CUser *user, void *data)
{
	SRemoveGPacket kRemoveGamePacket = *(SRemoveGPacket*)data;

	//Check if the user is a host
	if(user->GetGame() != NULL)
	{
		if(user->GetGame()->GetHost() == user)
		{
			sv->RemoveGame(user->GetGame());
		}
	}
}

void RecvPlayerVersion(CUser *user, void *data)
{
	SVersionCheckPacket kVersionPacket = *(SVersionCheckPacket*)data;

	user->SetVersion(kVersionPacket.Version);
}

void Analyze(uint8_t *data, uint32_t size, CNet *net)
{
	CUser *user = net->GetUser();
	if(user == NULL)
	{
		return;
	}

	uint16_t header = HEADER_OF(data);

	//Check what type of packet and if this user is allowed to send this
	if((header == CONNECT && !user->Allocated()) || user->Allocated() || header == VERSION_CHECK)
	{
		switch(header)
		{
		case CONNECT:
			{
				//Already allocated
				if(user->GetName().size())
				{
					break;
				}

				ParseConnect(user, data);

				break;
			}
		case HOST_GAME:
			{
				if(sv->CheckAnyHostIP(user->GetIp()))
				{
					break;
				}

				//Already in a game
				if(user->GetGame() != NULL)
				{
					break;
				}

				SendHostGame(user, data);

				break;
			}
		case JOIN_GAME:
			{
				//Already in a game
				if(user->GetGame() != NULL)
				{
					user->GetGame()->RemoveUser(user, CGame::EXIT);
				}

				RecvJoinGame(user, data);
				
				break;
			}
		case GAME_REFRESH:
			{
				RecvRefreshGame(user, data);

				break;
			}
		case GAME_LIST:
			{
				SendGameList(user);

				break;
			}
		case PLAYER_LIST:
			{
				SendPlayerList(user);

				break;
			}
		case CHAT:
			{
				//Check if this user can chat
				user->DoChat();
				if(!user->CanChat() || size > sizeof(SChatPacket) + MAX_CHAT_INPUT_CHAR)
				{
					break;
				}

				SendChat(user, data, size);
					
				break;
			}
		case CHANGE_NAME:
			{
				RecvChangeName(user, data);

				break;
			}
		case REJOIN_LOBBY:
			{
				user->SetGameStatus(STATUS_LOBBY);

				if(!user->Active())
				{
					SendGameList(user);
					SendPlayerList(user);

					user->SetActiveStatus(true);
				}

				break;
			}
		case GAME_STATUS:
			{
				SendPlayerGameStatus(user, data);

				break;
			}
		case GAME_REMOVE:
			{
				RecvRemoveGame(user, data);

				break;
			}
		case VERSION_CHECK:
			{
				RecvPlayerVersion(user, data);

				break;
			}
		case SIGNAL:
			{
				SendSignal(user, data);

				break;
			}
		default:
			{
				//Received invalid packet, block the ip
				sv->BlockIp(user->GetIp());

				printf("Received invalid packet. Header: %d\n", HEADER_OF(data));

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
		//check 
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

		//If not in lobby for this time, change active status to inactive
		if(GetRunTime()-m_InactiveTime > MAX_INACTIVE_TIME)
		{
			m_bActive = false;
		}

		//Reset the chat counter
		if(GetRunTime()-m_FirstChatTime > RESET_CHAT_COUNTER_TIME)
		{
			m_ChatCount = 0;
		}

		//if a connected user, update output
		if(m_Connection != NULL)
		{
			m_Connection->Update();

#ifdef UPDATE_USERS
			if(m_bAllocated && GetRunTime() > m_NextRefreshTime)
			{
				sv->UpdateUser(this);

				m_NextRefreshTime = GetRunTime() + 10000;
			}
#endif
		}
	}

//Send data without extra allocations. Data which hasn't been allocated manually will not be anymore on stack when function is called.
void CUser::AppendData(uint8_t *data, uint32_t size)
	{
		uint8_t* pData = NULL;

		if(m_Connection != NULL)
		{
			m_Connection->AddData(data, size, true);
		}
	}

//Send data without extra allocations. Data which hasn't been allocated manually will not be anymore on stack when function is called.
void CUser::AppendData(void *data, uint32_t size)
	{
		AppendData((uint8_t*)data, size);
	}

//Future use
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

//Future use
bool CUser::JoinGame(CGame *game)
	{
		if(game != NULL)
		{
			if(game->AddUser(this))
			{
				m_Game = game;
				return true;
			}
			else
			{
				m_Game = NULL;
			}
		}
		else
		{
			m_Game = NULL;
			return true;
		}

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

VERSION	CUser::GetVersion()
	{
		return m_Version;
	}

void CUser::SetName(std::string name)
	{
		m_Name = name;
		m_bAllocated = true;
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
		if(m_GameStatus != status)
		{
			if(status == STATUS_PLAYING)
			{
				m_InactiveTime = GetRunTime();
			}
			else
			{
				m_InactiveTime = 0;
			}
		}
		m_GameStatus = status;
	}

void CUser::SetVersion(VERSION version)
	{
		m_Version = version;
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

//Unused
uint16_t CUser::GetIGameId()
	{
		return m_IGameId;
	}

uint16_t CUser::GetGameId()
	{
		return m_GameId;
	}

//Connect succeed?
bool CUser::Allocated()
	{
		return m_bAllocated;
	}

bool CUser::Active()
{
	return m_bActive;
}

void CUser::SetActiveStatus(bool b)
	{
		m_bActive = b;
	}

//No spam please
void CUser::DoChat()
	{
		m_ChatCount++;
		if(m_ChatCount == 1)
		{
			m_FirstChatTime = GetRunTime();
		}
	}

//Block the chat for a given time
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

//Check if chat blocked
bool CUser::CanChat()
	{
		if(m_ChatBlockEnd == -1 || m_ChatCount > MAX_CHAT_COUNT ||
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

//Future use
void CUser::SetIGameId(uint16_t Id)
	{
		m_IGameId = Id;
	}

//Reallocation of memory, this functions posts a send function
void CUser::Send(uint8_t *data, uint32_t size)
	{
		uint8_t *pkData = Memcpy(data, size);
		m_Connection->AddData(pkData, size, false);
	}

//Reallocation of memory, this functions posts a send function
void CUser::Send(void *data, uint32_t size)
	{
		Send((uint8_t*)data, size);
	}

//Additional data to send with reallocation
void CUser::SendQueue(void *data, uint32_t size)
	{
		uint8_t *pkData = Memcpy((uint8_t*)data, size);
		//Post, so it will be sent after all outstanding "Send" functions
		sv->GetService().post([this, pkData, size]()
		{
			//this can be deallocated before hitting the function, very unsafe check, need to wait for this function before deleting
			if(sv->CheckUserExists(this))
			{
				m_SendQueue.push_back(std::make_pair(pkData, size));
			}
		});
	}

//Remove front pair
void CUser::QueuePopFront()
	{
		if(!m_SendQueue.empty())
		{
			m_SendQueue.pop_front();
		}
	}

std::pair<uint8_t *, uint32_t> *CUser::GetQueueData()
	{
		if(m_SendQueue.empty())
		{
			return NULL;
		}
		else
		{
			return &m_SendQueue.front();
		}
	}

/////////////////////////////

//     GAME

////////////////////////////


CGame::~CGame()
	{
		for(unsigned int i = 0; i < m_Users.size(); ++i)
		{
			if(m_Users[i] != NULL)
			{
				m_Users[i]->JoinGame(NULL);
			}
		}
	}

//Send updated playercount to everyone
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
		//Host is first
		if(m_Users.size())
		{
			return m_Users[0];
		}

		return NULL;
	}

uint16_t CGame::GetHostID()
	{
		//Host is first
		if(m_Users.size() > 0 && m_Users[0])
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

std::string CGame::GetGameName()
	{
		return m_Name;
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

uint8_t CGame::GetRankLevel()
	{
		return m_RankLevel;
	}

//Unused
PLAYERSTATS CGame::GetMinRank()
	{
		return m_MinRank;
	}

//Unused
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

//Future use
uint16_t CGame::GetNewIGameID()
	{
		int start = 1;
		int count = 0;
		for(unsigned int i = 0; i < m_Users.size(); ++i)
		{
			if(!m_Users[i])
			{
				continue;
			}
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
		//Host is first
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
				_user->Send((uint8_t*)&p, sizeof(SGRemovePPacket));
			}
		}

		m_Players--;

#endif
		user->SetGameId(-1);
		user->SetIGameId(-1);
		user->JoinGame(NULL);

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

void CCore::Initialize()
{
	LoadReservedNames();
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

//Update all users and games and check for console input
void CCore::Update()
	{
		std::for_each(UserList.begin(), UserList.end(), boost::bind(&CUser::Update, _1));
		std::for_each(GameList.begin(), GameList.end(), boost::bind(&CGame::Update, _1));

		m_Service.post(boost::bind(&CCore::Update, this));

		CConsole::Instance().Get();
	}

CCore::~CCore()
	{
		SShutDownPacket p = SShutDownPacket();

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

//Unused
void CCore::LoadReservedNames()
	{
	}

//Unused
void CCore::LoadAdmins()
	{
	}

//Unused
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

void CCore::SendToAll(uint8_t *data, uint32_t size, bool append)
	{
		BOOST_FOREACH(CUser *user, UserList)
		{
			//Only if the user has given response for the last hour
			if(user != NULL && user->Active())
			{
				if(append)
				{
					user->AppendData(data, size);
				}
				else
				{
					user->Send(data, size);
				}
			}
		}
	}

void CCore::SendToAll(void *data, uint32_t size, CUser *except, uint8_t status, VERSION *version, bool append)
	{
		BOOST_FOREACH(CUser *user, UserList)
		{
			if(user != NULL && user != except && user->Active())
			{
				if(status == UINT8_MAX || user->GetGameStatus() == status)
				{
					if(version != NULL)
					{
						if(user->GetVersion() != *version)
						{
							continue;
						}
					}
					if(append)
					{
						user->AppendData(data, size);
					}
					else
					{
						user->Send(data, size);
					}
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

bool CCore::CheckUserExists(CUser *user)
	{
		for(unsigned int i = 0; i < UserList.size(); ++i)
		{
			if(UserList[i] == user)
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

//Same as remove user
void CCore::RemoveConnection(CUser *user)
	{
		if(!user)
		{
			return;
		}
		if(user->Allocated())
		{
			SRemovePPacket kRemovePlayerPacket;
			kRemovePlayerPacket.id = user->GetId();

			SendToAll(&kRemovePlayerPacket, sizeof(SRemovePPacket));
		}
		std::deque<CUser *>::iterator it = std::find(UserList.begin(), UserList.end(), user);
		if(it != UserList.end())
		{
			UserList.erase(it);
		}
		delete user;
	}

//Same as remove user
void CCore::RemoveConnection(CNet *connection)
	{
		CUser *user = NULL;
		unsigned int pos = 0;

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
			if(user->Allocated())
			{
				SRemovePPacket kRemovePlayerPacket;
				kRemovePlayerPacket.id = user->GetId();

				SendToAll(&kRemovePlayerPacket, sizeof(SRemovePPacket));
			}

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

		SRemoveGPacket kRemoveGame;
		kRemoveGame.id = game->GetGameID();
		SendToAll(&kRemoveGame, sizeof(SRemoveGPacket));

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
			else if(split[0] == "ServerMessage" && split.size() > 1)
			{
				sstrcpy(ServerMessage, split[1].c_str());

				printf("Server message is now: %s\n", split[1].c_str());
			}
		}
	}
