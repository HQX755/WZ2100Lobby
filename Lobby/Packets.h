#ifndef _PACKETS_H_
#define _PACKETS_H_

#include "stdafx.h"
#include "Structs.h"
#include "Utils.h"

#define MAX_GAME_NAME_LENGTH 20
#define MAX_PLAYER_NAME_LENGTH 20
#define MAX_PASSWORD_LENGTH 20

enum EPacketHeaders
{
	CONNECT = 1,
	CHAT = 2,
	COMMAND = 3,
	HOST_GAME = 4,
	GAME_HOSTED = 5,
	PLAYER_ADD = 6,
	GAME_ADD = 7,
	GAME_STATUS = 8,
	PLAYER_REMOVE = 9,
	GAME_REMOVE = 10,
	PLAYER_DATA = 11,
	GAME_REFRESH = 12,
	CHANGE_NAME = 13,
	REQUEST_PASSWORD = 14,
	GAME_PLAYER_REMOVE = 15,
	PLAYER_REFRESH = 16,
	REJOIN_LOBBY = 17,
	GAME_REMOVE_PLAYER = 18,
	JOIN_GAME = 19
};

enum EPacketSubHeaders
{
	BAN_PLAYER = 0xFB,
	BAN_GAME = 0xFE,
	SEND_NOTICE = 0xFF
};

enum EConnectionState
{
	CONNECTION_ERROR,
	CONNECTION_SUCCESS,
	CONNECTION_NONE
};

enum EGameState
{
	GAME_INLOBBY,
	GAME_INGAME
};

enum EGameStatus
{
	STATUS_LOBBY,
	STATUS_PLAYING
};

enum EUserAuthority
{
	STATUS_PLAYER,
	STATUS_REGISTERED,
	STATUS_ADMIN
};

struct SShutDownPacket
{
	SShutDownPacket() : header(0xDEAD)
	{
	}
	uint16_t header;
};

struct SChatPacket
{
	SChatPacket(const char *txt, uint16_t Id) : header(CHAT), id(Id)
	{
		memcpy(Text, txt, 128);
	}
	SChatPacket() : header(CHAT), id(0)
	{
		memset(Text, 0, 128);
	}
	uint16_t header;
	uint16_t id;
	char Text[128];
};

struct SNewPPacket
{
	SNewPPacket(const char *name, PLAYERSTATS rank) : header(PLAYER_ADD), id(0), status(0), playing(STATUS_LOBBY),
		Rank(rank)
	{
		memcpy(Name, name, MAX_PLAYER_NAME_LENGTH);
	}
	SNewPPacket() : header(PLAYER_ADD), id(0), status(0), playing(STATUS_LOBBY)
	{
		memset(Name, 0, MAX_PLAYER_NAME_LENGTH);
	}
	uint16_t header;
	uint16_t id;
	char Name[20];
	PLAYERSTATS Rank;
	uint8_t status;
	uint8_t playing;
};

struct SConnectPacket
{
	SConnectPacket(const char *name, PLAYERSTATS rank) : header(CONNECT), id(0), Rank(rank)
	{
		sstrcpy(Name, name);
	}
	SConnectPacket() : header(CONNECT), id(0)
	{
		memset(Name, 0, MAX_PLAYER_NAME_LENGTH);
	}
	uint16_t header;
	uint16_t id;
	char Name[20];
	PLAYERSTATS Rank;
};

struct SPlayerDataPacket
{
	SPlayerDataPacket(uint16_t Id, uint8_t Status) : header(PLAYER_DATA), id(Id), status(Status)
	{
	}
	SPlayerDataPacket() : header(PLAYER_DATA), id(0), status(STATUS_PLAYER)
	{
	}
	uint16_t header;
	uint16_t id;
	uint8_t status;
};

struct SNewGPacket
{
	SNewGPacket(uint16_t Id, uint16_t HostId, const char *name, const char *map, const char *Mods, 
		const char *Ip, const char *version, uint8_t players, uint8_t maxPlayers, PLAYERSTATS minRank, PLAYERSTATS maxRank,
		uint32_t version_major, uint32_t version_minor, uint8_t MapMod, uint8_t PrivateGame) : 
		header(GAME_ADD), 
		id(Id),
		hostId(HostId),
		Players(players),
		MaxPlayers(maxPlayers),
		game_version_major(version_major),
		game_version_minor(version_minor),
		mapMod(MapMod),
		privateGame(PrivateGame),
		MinRank(minRank),
		MaxRank(maxRank)
	{
		sstrcpy(Name, name);
		sstrcpy(ip, Ip);
		sstrcpy(Map, map);
		sstrcpy(Version, version);
		sstrcpy(mods, Mods);
	};
	SNewGPacket() : 
		header(GAME_ADD),
		id(0),
		hostId(0),
		Players(0),
		MaxPlayers(0),
		game_version_major(0),
		game_version_minor(0),
		mapMod(0),
		privateGame(0)
	{
	}
	uint16_t header;
	uint16_t id;
	uint16_t hostId;
	char Name[20];
	char ip[20];
	char Map[20];
	char Version[10];
	char mods[20];
	uint8_t Players;
	uint8_t MaxPlayers;
	PLAYERSTATS MinRank;
	PLAYERSTATS MaxRank;
	uint32_t game_version_major;
	uint32_t game_version_minor;
	uint8_t mapMod;
	uint8_t privateGame;
};

struct SRemovePPacket
{
	SRemovePPacket(uint16_t Id) : header(PLAYER_REMOVE), id(Id)
	{
	}
	SRemovePPacket() : header(PLAYER_REMOVE), id(0)
	{
	}
	uint16_t header;
	uint16_t id;
};

struct SRemoveGPacket
{
	SRemoveGPacket(uint16_t Id) : header(GAME_REMOVE), id(Id)
	{
	}
	SRemoveGPacket() : header(GAME_REMOVE), id(0)
	{
	}
	uint16_t header;
	uint16_t id;
};

struct SGRemovePPacket
{
	SGRemovePPacket(uint16_t Id, uint8_t Reason) : header(GAME_REMOVE_PLAYER), id(Id), reason(Reason)
	{
	}
	SGRemovePPacket() : header(GAME_REMOVE_PLAYER), id(0), reason(0)
	{
	}
	uint16_t header;
	uint16_t id;
	uint8_t reason;
};

struct SChangeNamePacket
{
	SChangeNamePacket(uint16_t Id, const char* Name, PLAYERSTATS Stats) : header(CHANGE_NAME), id(Id), stats(Stats)
	{
		sstrcpy(name, Name);
	}
	SChangeNamePacket() : header(CHANGE_NAME), id(0)
	{
		memset(name, 0, MAX_GAME_NAME_LENGTH);
		memset(&stats, 0, sizeof(PLAYERSTATS));
	}
	uint16_t header;
	uint16_t id;
	char name[20];
	PLAYERSTATS stats;
};

struct SGameStatusPacket
{
	SGameStatusPacket(uint16_t Id, uint8_t Status) : header(GAME_STATUS), id(Id), status(Status)
	{
	}
	SGameStatusPacket() : header(GAME_STATUS), id(0), status(STATUS_LOBBY)
	{
	}
	uint16_t header;
	uint16_t id;
	uint8_t status;
};

struct SGameHostPacket
{
	SGameHostPacket(uint16_t id) : header(HOST_GAME), gId(id)
	{
	}
	SGameHostPacket() : header(HOST_GAME), gId(0)
	{
	}
	uint16_t header;
	uint16_t gId;
};

struct SRefreshGamePacket
{
	SRefreshGamePacket(uint16_t id, uint8_t Players, uint8_t MaxPlayers) : header(GAME_REFRESH), gId(id), players(Players), 
		maxPlayers(MaxPlayers)
	{
	}
	SRefreshGamePacket() : header(GAME_REFRESH), gId(0), players(0), maxPlayers(0)
	{
	}
	uint16_t header;
	uint16_t gId;
	uint8_t players;
	uint8_t maxPlayers;
};

struct SRefreshPlayerPacket
{
	SRefreshPlayerPacket(uint16_t Id, uint8_t Playing) : header(PLAYER_REFRESH), id(Id), playing(Playing)
	{
	}
	SRefreshPlayerPacket() : header(PLAYER_REFRESH), id(0), playing(STATUS_LOBBY)
	{
	}
	uint16_t header;
	uint16_t id;
	uint8_t playing;
};

struct SRequestPasswordPacket
{
	SRequestPasswordPacket(const char *Name, const char *Password) : header(REQUEST_PASSWORD)
	{
		sstrcpy(name, Name);
		sstrcpy(password, Password);
	}
	SRequestPasswordPacket() : header(REQUEST_PASSWORD)
	{
		memset(name, 0, MAX_PLAYER_NAME_LENGTH);
		memset(password, 0, MAX_PASSWORD_LENGTH);
	}
	uint16_t header;
	char name[20];
	char password[20];
};

struct SJoinGamePacket
{
	SJoinGamePacket(uint16_t id) : header(JOIN_GAME), gId(id)
	{
	}
	SJoinGamePacket() : header(JOIN_GAME), gId(0)
	{
	}
	uint16_t header;
	uint16_t gId;
};
#endif