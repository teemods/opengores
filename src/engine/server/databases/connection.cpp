#include "connection.h"

#include <engine/shared/protocol.h>

void IDbConnection::FormatCreateRace(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_race ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time FLOAT DEFAULT 0, "
		"  Server CHAR(4), "
		"  cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, "
		"  cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, "
		"  cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, "
		"  cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, "
		"  cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, "
		"  cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, "
		"  cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, "
		"  cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, "
		"  cp25 FLOAT DEFAULT 0, "
		"  GameID VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (Map, Name, Time, Timestamp, Server)"
		")",
		GetPrefix(), BinaryCollate(), MAX_NAME_LENGTH, BinaryCollate());
}

void IDbConnection::FormatCreateTeamrace(char *aBuf, unsigned int BufferSize, const char *pIdType)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_teamrace ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Time FLOAT DEFAULT 0, "
		"  ID %s NOT NULL, " // VARBINARY(16) for MySQL and BLOB for SQLite
		"  GameID VARCHAR(64), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  PRIMARY KEY (ID, Name)"
		")",
		GetPrefix(), BinaryCollate(), MAX_NAME_LENGTH, BinaryCollate(), pIdType);
}

void IDbConnection::FormatCreateMaps(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_maps ("
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Server VARCHAR(32) COLLATE %s NOT NULL, "
		"  Mapper VARCHAR(128) COLLATE %s NOT NULL, "
		"  Points INT DEFAULT 0, "
		"  Stars INT DEFAULT 0, "
		"  Timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
		"  PRIMARY KEY (Map)"
		")",
		GetPrefix(), BinaryCollate(), BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreateSaves(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_saves ("
		"  Savegame TEXT COLLATE %s NOT NULL, "
		"  Map VARCHAR(128) COLLATE %s NOT NULL, "
		"  Code VARCHAR(128) COLLATE %s NOT NULL, "
		"  Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"  Server CHAR(4), "
		"  DDNet7 BOOL DEFAULT FALSE, "
		"  SaveID VARCHAR(36) DEFAULT NULL, "
		"  PRIMARY KEY (Map, Code)"
		")",
		GetPrefix(), BinaryCollate(), BinaryCollate(), BinaryCollate());
}

void IDbConnection::FormatCreatePoints(char *aBuf, unsigned int BufferSize)
{
	str_format(aBuf, BufferSize,
		"CREATE TABLE IF NOT EXISTS %s_points ("
		"  Name VARCHAR(%d) COLLATE %s NOT NULL, "
		"  Points INT DEFAULT 0, "
		"  SeasonPoints INT DEFAULT 0, "
		"  Rainbow TINYINT DEFAULT 0, "
		"  RainbowEnabled TINYINT DEFAULT 1, "
		"  RainbowBlack TINYINT DEFAULT 0, "
		"  RainbowBlackEnabled TINYINT DEFAULT 1, "
		"  Splash TINYINT DEFAULT 0, "
		"  SplashEnabled TINYINT DEFAULT 1, "
		"  Explosion TINYINT DEFAULT 0, "
		"  ExplosionEnabled TINYINT DEFAULT 1, "
		"  SplashPistol TINYINT DEFAULT 0, "
		"  SplashPistolEnabled TINYINT DEFAULT 1, "
		"  ExplosionPistol TINYINT DEFAULT 0, "
		"  ExplosionPistolEnabled TINYINT DEFAULT 1, "
		"  Star TINYINT DEFAULT 0, "
		"  StarEnabled TINYINT DEFAULT 1, "
		"  AuraDot TINYINT DEFAULT 0, "
		"  AuraDotEnabled TINYINT DEFAULT 1, "
		"  AuraGun TINYINT DEFAULT 0, "
		"  AuraGunEnabled TINYINT DEFAULT 1, "
		"  AuraShotgun TINYINT DEFAULT 0, "
		"  AuraShotgunEnabled TINYINT DEFAULT 1, "
		"  Trail TINYINT DEFAULT 0, "
		"  TrailEnabled TINYINT DEFAULT 1, "
		"  Emotion TINYINT DEFAULT 0, "
		"  Soundtrack TINYINT DEFAULT 0, "
		"  DropHeart TINYINT DEFAULT 0, "
		"  DropShield TINYINT DEFAULT 0, "
		"  DropNinjaSword TINYINT DEFAULT 0, "
		"  GuidedHeart TINYINT DEFAULT 0, "
		"  GuidedShield TINYINT DEFAULT 0, "
		"  GuidedNinjaSword TINYINT DEFAULT 0, "
		"  Carry TINYINT DEFAULT 0, "
		"  PRIMARY KEY (Name)"
		")",
		GetPrefix(), MAX_NAME_LENGTH, BinaryCollate());
}
