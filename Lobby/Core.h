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

#define ADMIN_TXT_FILE "admins.txt"
#define ACCOUNT_TXT_FILE "users.txt"

#define MAX_PLAYER_PER_IP 10
#define GAME_START_ID 10201
#define MAX_GAMES_PER_IP 1
#define MAX_GAMES 11
#define MAX_USERS 1024

static uint16_t PlayerCount = 0;
static uint32_t GameCount = 0;

uint16_t GeneratePlayerID();
uint32_t GenerateGameID();

void ServerInit();
void CreateListener(boost::asio::io_service &service);
void StartAccept();
void AcceptCallback(CNet *net);
void Analyze(uint8_t *data, uint32_t size, CNet *net);

enum EGameResponse
{
	GAME_FULL,
	GAME_PASSWORD_REQUIRED
};

enum EUserAuthority
{
	STATUS_PLAYER,
	STATUS_REGISTERED,
	STATUS_ADMIN
};

class CGame;

class CUser
{
private:
	std::string m_Name;
	std::string m_Ip;

	unsigned long m_ChatCount;
	unsigned long m_ChatBlockEnd;
	unsigned long m_FirstChatTime;
	unsigned long m_JoinTime;

	uint16_t	m_Id;
	uint16_t	m_IGameId;
	uint16_t	m_GameId;
	uint32_t	m_Rank[7];
	uint8_t		m_Status;

	bool		m_Allocated;

	CGame	*m_Game;
	CNet	*m_Connection;

public:

	CUser(CNet *net, std::string Ip) : m_Connection(net), m_Ip(Ip), m_Game(NULL), m_Status(STATUS_PLAYER),
		m_ChatCount(0),
		m_FirstChatTime(0),
		m_Id(-1),
		m_IGameId(-1),
		m_GameId(-1),
		m_Allocated(false),
		m_ChatBlockEnd(0)
	{
		Initialize();
	}

	~CUser();

	void Initialize()
	{
		m_JoinTime = GetRunTime();
		m_Id = GeneratePlayerID();

		memset(&m_Rank, 0x00, sizeof(uint32_t) * 7);
		m_Connection->BindUser(this);
	}

	void Update();
	void AppendData(uint8_t *data, uint32_t size);
	void AppendData(void *data, uint32_t size);

	int TryJoinGame(CGame *game);

	void JoinGame(CGame *game);
	CGame *HostGame();

	uint16_t GetId();
	uint16_t GetIGameId();
	uint32_t GetGameId();
	uint32_t GetLastChatCount();
	uint8_t GetStatus();

	void SetName(std::string name);
	void SetRank(uint32_t rank[7]);
	void SetStatus(EUserAuthority status);

	std::string GetName();
	std::string GetIp();
	uint32_t *GetRank();

	bool Allocated();

	void DoChat();
	void BlockChat(unsigned long dwTime = -1);
	bool CanChat();

	void SetGameId(uint16_t Id);
	void SetIGameId(uint16_t Id);

	CGame *GetGame();
	CNet *GetConnection();

	void SetConnection(CNet *conn);
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

	uint32_t m_GameId;
	uint32_t m_Players;
	uint32_t m_MaxPlayers;
	uint32_t m_MinRank[7];
	uint32_t m_MaxRank[7];

	UserVec m_Users;

public:

	enum ERemoveReason
	{
		UNWANTED_BY_HOST,
		MODDED_GAME,
		UNKNOWN_COMMAND,
		EXIT
	};

	CGame(const char* name, const char* map, int MaxPlayers, const char* version, const char* mods, const char *password) :
		m_Name(name),
		m_Map(map),
		m_Players(0),
		m_MaxPlayers(MaxPlayers),
		m_Version(version),
		m_Password(password),
		m_Mods(mods)
	{
		Initialize();
	}

	void Initialize()
	{
		m_GameId = GenerateGameID();
		m_NextRefreshTime = GetRunTime() + 10000;

		memset(&m_MinRank, 0, sizeof(uint32_t) * 7);
		memset(&m_MaxRank, 0, sizeof(uint32_t) * 7);
	}

	~CGame();

	void Update();

	std::string GetMapName();
	std::string GetGamePassword();
	std::string GetMods();

	bool IsFull();
	bool IsHost(CUser *user);

	void RemoveUser(CUser *user, CGame::ERemoveReason reason);
	void AddUser(CUser *user);
	void Close();

	CUser *GetHost();
	uint32_t GetGameID();
	uint16_t GetHostID();
	uint32_t GetMaxPlayers();
	uint32_t GetPlayerCount();
	uint32_t GetNewIGameID();

	uint32_t *GetMinRank();
	uint32_t *GetMaxRank();
};

enum EErrorCodes
{
	ERROR_NOERROR,
	ERROR_CONNECT_FAILURE
};

SShutDownPacket ShutDown();

class CCore
{
private:
	std::deque<CUser *>		UserList;
	std::deque<CGame *>		GameList;
	std::deque<std::string>	BlockList;
	std::deque<std::string>	PlayerList;
	std::deque<std::string> AdminList;

	EErrorCodes LastError;

	boost::thread *m_NetThread;
	boost::mutex m_Mutex;

	boost::asio::io_service m_Service;

public:
	boost::asio::io_service &GetService();

	CCore() : m_NetThread(NULL), m_Service(), UserList(), GameList()
	{
		Initialize();
	}

	void Initialize()
	{
		LoadReservedNames();
		CreateListener(m_Service);
	}

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
	void SendToAll(uint8_t *data, uint32_t size);
	void SendToAll(void *data, uint32_t size, CUser *except = NULL);

	void BlockIp(std::string ip);
	void UnblockIp(std::string ip);
	bool IpIsBlocked(std::string ip);
	bool CheckAnyHostIP(std::string ip);

	bool MaxConnectionsReached(std::string ip);

	int NameAlreadyExist(std::string name);

	std::vector<CUser *> GetUsersByIP(std::string ip);
	std::vector<CUser *> GetUsersByName(std::string name);

	std::deque<CUser *> *GetUserList();
	std::deque<CGame *> *GetGameList();

	CUser *GetUserByName(const char *name);
	CUser *GetUserByNet(CNet *net);
	CUser *GetUserById(uint16_t id);

	void RemoveConnection(CUser *user);
	void RemoveConnection(CNet *connection);
	void RemoveGame(CGame *game);
	void UpdateGame(CGame *game);

	void Command(std::string cmd);
};

CCore *CORE();

class CConsole
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
	}

	void Initialize()
	{
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