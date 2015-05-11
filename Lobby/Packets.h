#ifndef _PACKETS_H_
#define _PACKETS_H_

#include "stdafx.h"

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
	REJOIN_LOBBY = 17
};

enum EPacketSubHeaders
{
	BAN_PLAYER = 0xFB,
	BAN_GAME = 0xFE,
	SEND_NOTICE = 0xFF
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
	uint16_t header;
	uint16_t id;
	char Text[128];
};

struct SNewPPacket
{
	uint16_t header;
	uint16_t id;
	char Name[20];
	uint32_t Rank[7];
	uint8_t status;
	uint8_t playing;
};

struct SConnectPacket
{
	uint16_t header;
	uint16_t id;
	char Name[20];
	uint32_t Rank[7];
};

struct SPlayerDataPacket
{
	uint16_t header;
	uint16_t id;
	uint8_t status;
};

struct SNewGPacket
{
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
	uint32_t MinRank[7];
	uint32_t MaxRank[7];
	uint32_t game_version_major;
	uint32_t game_version_minor;
	uint8_t mapMod;
	uint8_t privateGame;
};

struct SRemovePPacket
{
	uint16_t header;
	uint16_t id;
};

struct SRemoveGPacket
{
	uint16_t header;
	uint16_t id;
};

struct SGRemovePPacket
{
	uint16_t header;
	uint16_t id;
	uint8_t reason;
};

struct SChangeNamePacket
{
	uint16_t header;
	uint16_t id;
	char name[20];
};

struct SGameStatusPacket
{
	uint16_t header;
	uint8_t status;
};

struct SGameHostPacket
{
	uint16_t header;
	uint16_t gId;
};

struct SRefreshGamePacket
{
	uint16_t header;
	uint16_t gId;
	uint8_t players;
};

struct SRefreshPlayerPacket
{
	uint16_t header;
	uint16_t id;
	uint8_t playing;
};

struct SRequestPasswordPacket
{
	uint16_t header;
	char name[20];
	char password[20];
};

struct SJoinGamePacket
{
	uint16_t header;
	uint16_t gId;
};

#endif