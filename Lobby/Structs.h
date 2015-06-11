#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include "stdafx.h"

#define MaxMsgSize		16384
#define	StringSize		64
#define MaxGames		11
#define extra_string_size	159
#define map_string_size		40
#define	hostname_string_size	40
#define modlist_string_size	255
#define password_string_size 64

#define MAX_CONNECTED_PLAYERS   MAX_PLAYERS
#define MAX_TMP_SOCKETS         16

struct SESSIONDESC 
{
	int32_t dwSize;
	int32_t dwFlags;
	char host[40];
	int32_t dwMaxPlayers;
	int32_t dwCurrentPlayers;
	int32_t dwUserFlags[4];
};

struct GAMESTRUCT
{
	uint32_t	GAMESTRUCT_VERSION;

	char		name[StringSize];
	SESSIONDESC	desc;

	char		secondaryHosts[2][40];
	char		extra[extra_string_size];
	char		mapname[map_string_size];
	char		hostname[hostname_string_size];
	char		versionstring[StringSize];
	char		modlist[modlist_string_size];
	uint32_t	game_version_major;
	uint32_t	game_version_minor;
	uint32_t	privateGame;
	uint32_t	pureMap;
	uint32_t	Mods;

	uint32_t	gameId;
	uint32_t	limits;
	uint32_t	future3;
	uint32_t	future4;
};

struct PLAYERSTATS
{
	uint32_t played;
	uint32_t wins;
	uint32_t losses;
	uint32_t totalKills;
	uint32_t totalScore;

	uint32_t recentKills;
	uint32_t recentScore;
};

struct VERSION
{
	uint32_t versionMinor;
	uint32_t versionMajor;
	char	 versionString[10];
	bool operator==(VERSION &other)
	{
		if(other.versionMinor == versionMinor && other.versionMajor == versionMajor &&
			strcmp(other.versionString, versionString) == 0)
		{
			return true;
		}

		return false;
	}
	bool operator!=(VERSION &other)
	{
		if(other.versionMinor != versionMinor || other.versionMajor != versionMajor ||
			strcmp(other.versionString, versionString) != 0)
		{
			return true;
		}

		return false;
	}
};

#endif