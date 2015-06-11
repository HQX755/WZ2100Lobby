#ifndef _CORE_H_
#define _CORE_H_

#if defined(_MSC_VER) && _MSC_VER >= 1400
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#include "stdafx.h"
#include "Packets.h"
#include "Net.h"
#include "Utils.h"

#define MAX_PLAYER_PER_IP		10
#define GAME_START_ID			10201
#define MAX_GAMES_PER_IP		1
#define MAX_GAMES				11
#define MAX_USERS				1024
#define MAX_PLAYERS				1024
#define MAX_CHAT_COUNT			10

#define SERVER_MESSAGE "Welcome to the lobby server."

#define RESET_CHAT_COUNTER_TIME 10000
#define MAX_INACTIVE_TIME		3600000

static uint16_t PlayerCount = 0;
static uint8_t GameCount = 0;

uint16_t GeneratePlayerID();
uint16_t GenerateGameID();

void ServerInit();
void Stop();

void CreateListener(boost::asio::io_service &service);
void StartAccept();
void AcceptCallback(CNet *net);

void Analyze(uint8_t *data, uint32_t size, CNet *net);

enum EGameResponse
{
	GAME_FULL,
	GAME_PASSWORD_REQUIRED
};

class CGame;

class CUser
{
private:
	std::string m_Name;
	std::string m_Ip;

	enum NETVersion
	{
		NET_VERSION_MINOR,
		NET_VERSION_MAJOR
	};

	unsigned long m_ChatCount;
	unsigned long m_ChatBlockEnd;
	unsigned long m_FirstChatTime;
	unsigned long m_JoinTime;
	unsigned long m_NextRefreshTime;
	unsigned long m_InactiveTime;

	uint16_t	m_Id;
	uint16_t	m_IGameId;
	uint16_t	m_GameId;
	uint8_t		m_Status;
	uint8_t		m_GameStatus;

	PLAYERSTATS	m_Rank;
	VERSION		m_Version;

	bool		m_bAllocated;
	bool		m_bActive;

	CGame	*m_Game;
	CNet	*m_Connection;

	std::deque<std::pair<uint8_t*, uint32_t>> m_SendQueue;

public:

	CUser(CNet *net, std::string Ip) : 
		m_Connection(net), 
		m_Ip(Ip), 
		m_Game(NULL), 
		m_Status(STATUS_PLAYER),
		m_GameStatus(STATUS_LOBBY),
		m_ChatCount(0),
		m_FirstChatTime(0),
		m_Id(-1),
		m_IGameId(-1),
		m_GameId(-1),
		m_bAllocated(false),
		m_bActive(true),
		m_ChatBlockEnd(0),
		m_NextRefreshTime(0),
		m_InactiveTime(0)
	{
		Initialize();
	}

	~CUser();

	void Initialize()
	{
		m_JoinTime = GetRunTime();
		m_Id = GeneratePlayerID();

		m_NextRefreshTime = GetRunTime() + 10000;

		memset(&m_Rank, 0x00, sizeof(PLAYERSTATS));
		memset(&m_Version, 0x00, sizeof(VERSION));

		m_Connection->BindUser(this);
	}

	void Create(CNet *net, std::string ip)
	{
		m_Connection = net;
		m_Ip = ip;
		m_Game = NULL;
		m_GameStatus = STATUS_LOBBY;
		m_Status = STATUS_PLAYER;
		m_ChatCount = 0;
		m_FirstChatTime = 0;
		m_Id = -1;
		m_IGameId = -1;
		m_GameId = -1;
		m_bAllocated = false;
		m_bActive = true;
		m_ChatBlockEnd = 0;
		m_NextRefreshTime = 0;
		m_InactiveTime = 0;

		Initialize();
	}

	void Update();
	void AppendData(uint8_t *data, uint32_t size);
	void AppendData(void *data, uint32_t size);

	int		TryJoinGame(CGame *game);
	bool	JoinGame(CGame *game);

	CGame	*HostGame();

	uint16_t	GetId();
	uint16_t	GetIGameId();
	uint16_t	GetGameId();
	uint32_t	GetLastChatCount();
	uint8_t		GetStatus();
	uint8_t		GetGameStatus();

	void SetName(std::string name);
	void SetRank(PLAYERSTATS rank);
	void SetStatus(EUserAuthority status);
	void SetGameStatus(EGameStatus status);
	void SetVersion(VERSION version);

	std::string GetName();
	std::string GetIp();

	PLAYERSTATS GetRank();
	VERSION		GetVersion();

	bool Allocated();
	bool Active();

	void SetActiveStatus(bool b);

	void DoChat();
	void BlockChat(unsigned long dwTime = -1);
	bool CanChat();

	void SetGameId(uint16_t Id);
	void SetIGameId(uint16_t Id);

	CGame	*GetGame();
	CNet	*GetConnection();

	void SetConnection(CNet *conn);
	void Send(uint8_t *data, uint32_t size);
	void Send(void *data, uint32_t size);
	void SendQueue(void *data, uint32_t size);
	void QueuePopFront();

	std::pair<uint8_t *, uint32_t> *GetQueueData();
};

typedef std::vector<CUser *> UserVec;

class CGame
{
private:
	unsigned long m_NextRefreshTime;

	std::string m_Name;
	std::string m_Version;
	std::string m_Map;
	std::string m_Password;
	std::string m_Mods;

	uint32_t	m_GameId;
	uint8_t		m_Players;
	uint8_t		m_MaxPlayers;
	uint8_t		m_RankLevel;

	PLAYERSTATS m_MinRank;
	PLAYERSTATS m_MaxRank;

	UserVec m_Users;

	bool m_Modded;
	bool m_Private;
	bool m_InLobby;

public:

	enum ERemoveReason
	{
		UNWANTED_BY_HOST,
		MODDED_GAME,
		UNKNOWN_COMMAND,
		EXIT
	};

	CGame(const char* name, const char* map, int MaxPlayers, const char* version, const char* mods, 
		const char *password, bool mod, bool privateGame, uint8_t rank) :
		m_Name(name),
		m_Map(map),
		m_Players(0),
		m_MaxPlayers(MaxPlayers),
		m_Version(version),
		m_Password(password),
		m_Mods(mods),
		m_Modded(mod),
		m_Private(privateGame),
		m_InLobby(true),
		m_RankLevel(rank)
	{
		Initialize();
	}

	void Initialize()
	{
		m_GameId = GenerateGameID();
		m_NextRefreshTime = GetRunTime() + 10000;

		memset(&m_MinRank, 0, sizeof(PLAYERSTATS));
		memset(&m_MaxRank, 0, sizeof(PLAYERSTATS));
	}

	~CGame();

	void Update();

	std::string GetMapName();
	std::string GetGameName();
	std::string GetGamePassword();
	std::string GetMods();

	bool IsFull();
	bool IsHost(CUser *user);
	bool HasMapMod();
	bool IsPrivate();

	void RemoveUser(CUser *user, CGame::ERemoveReason reason);
	bool AddUser(CUser *user);
	void Close();
	void StartGame();

	void SetPlayerCount(uint8_t players);
	void SetMaxPlayerCount(uint8_t max);

	CUser *GetHost();

	uint16_t	GetGameID();
	uint16_t	GetHostID();
	uint16_t	GetNewIGameID();

	uint8_t		GetMaxPlayers();
	uint8_t		GetPlayerCount();
	uint8_t		GetRankLevel();

	PLAYERSTATS GetMinRank();
	PLAYERSTATS GetMaxRank();
};

enum EErrorCodes
{
	ERROR_NOERROR,
	ERROR_CONNECT_FAILURE
};

SShutDownPacket ShutDown();

class CCore : public CInstance<CCore>
{
private:
	std::deque<CUser *>		UserList;
	std::deque<CGame *>		GameList;
	std::deque<std::string>	BlockList;
	std::deque<std::string>	PlayerList;
	std::deque<std::string> AdminList;

	EErrorCodes LastError;

	boost::thread	m_NetThread;
	boost::mutex	m_Mutex;

	boost::asio::io_service &m_Service;

public:
	boost::asio::io_service &GetService();

	CCore(boost::asio::io_service &service) : 
		m_Service(service), 
		LastError(ERROR_NOERROR)
	{
		Initialize();
	}

	void Initialize();
	void Run();

	void AddUser(CUser *user);
	void AddGame(CGame *game);
	void Update();

	~CCore();

	int CheckUserStatus(const char *name);

	void LoadReservedNames();
	void LoadAdmins();

	void BanGameFromLobby(CGame *game);

	void SendTo(CUser *user, uint8_t *data, uint32_t size);
	void SendTo(CUser *user, void *data, uint32_t size);
	void SendToAll(uint8_t *data, uint32_t size, bool append = false);
	void SendToAll(void *data, uint32_t size, CUser *except = NULL, uint8_t status = UINT8_MAX, VERSION *version = NULL, bool append = false);

	void BlockIp(std::string ip);
	void UnblockIp(std::string ip);
	bool IpIsBlocked(std::string ip);
	bool CheckAnyHostIP(std::string ip);
	bool CheckUserExists(CUser *user);

	bool MaxConnectionsReached(std::string ip);

	int NameAlreadyExist(std::string name);

	std::vector<CUser *> GetUsersByIP(std::string ip);
	std::vector<CUser *> GetUsersByName(std::string name);

	std::deque<CUser *> *GetUserList();
	std::deque<CGame *> *GetGameList();

	CUser *GetUserByName(const char *name);
	CUser *GetUserByNet(CNet *net);
	CUser *GetUserById(uint16_t id);

	CGame *GetGameById(uint16_t id);

	void RemoveConnection(CUser *user);
	void RemoveConnection(CNet *connection);
	void RemoveGame(CGame *game);

	void UpdateUser(CUser *user);
	void UpdateGame(CGame *game);

	void Command(std::string cmd);
};

CCore *CORE();

class CConsole : public CInstance<CConsole>
{
private:
	std::deque<std::string> m_CommandVec;

	boost::mutex m_Mutex;
	boost::condition_variable m_Condition;

	boost::thread *m_InputThread;

public:
	CConsole() : m_CommandVec(128)
	{
		Initialize();
	}

	~CConsole()
	{
		if(m_InputThread != NULL)
		{
			Stop();
			delete m_InputThread;
		}
	}

	void Initialize()
	{
		m_InputThread = NULL;
	}

	void Run()
	{
		m_InputThread = new boost::thread(boost::bind(&CConsole::Read, this));
	}

	void Read()
	{
		while(true)
		{
			std::string str;
			std::getline(std::cin, str);

			Put(str.c_str());
		}
	}

	void Stop()
	{
		m_InputThread->interrupt();
	}

	void Put(const char *data)
	{
		try
		{
			boost::lock_guard<boost::mutex> lock(m_Mutex);

			m_CommandVec.push_back(std::string(data));
		}
		catch(std::exception &e)
		{
			printf("Failed by adding data to commandlist. %s\n", e.what());
		}
	}

	void Get()
	{
		boost::lock_guard<boost::mutex> lock(m_Mutex);
		{
			while(!m_CommandVec.empty())
			{
				CORE()->Command(m_CommandVec[0]);
				m_CommandVec.pop_front();
			}
		}
	}
};

#if defined(_MSC_VER) && _MSC_VER >= 1400
#pragma warning(pop)
#endif

#endif