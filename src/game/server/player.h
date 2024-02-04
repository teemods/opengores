/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/alloc.h>
#include <game/server/save.h>

#include "teeinfo.h"

#include <memory>
#include <optional>

class CCharacter;
class CGameContext;
class IServer;
struct CNetObj_PlayerInput;
struct CScorePlayerResult;

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, uint32_t UniqueClientID, int ClientID, int Team);
	~CPlayer();

	void Reset();

	void TryRespawn();
	void Respawn(bool WeakHook = false); // with WeakHook == true the character will be spawned after all calls of Tick from other Players
	CCharacter *ForceSpawn(vec2 Pos); // required for loading savegames
	void SetTeam(int Team, bool DoChatMsg = true);
	int GetTeam() const { return m_Team; }
	int GetCID() const { return m_ClientID; }
	uint32_t GetUniqueCID() const { return m_UniqueClientID; }
	int GetClientVersion() const;
	bool SetTimerType(int TimerType);

	void Tick();
	void PostTick();

	// will be called after all Tick and PostTick calls from other players
	void PostPostTick();
	void Snap(int SnappingClient);
	void FakeSnap();

	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedEarlyInput(CNetObj_PlayerInput *pNewInput);
	void OnDisconnect();

	void KillCharacter(int Weapon = WEAPON_GAME, bool SendKillMsg = true);
	CCharacter *GetCharacter();
	const CCharacter *GetCharacter() const;

	void SpectatePlayerName(const char *pName);

	// OpenGores
	void ProcessPauseEffect();
	bool DropLoot(int LootType, bool Guided);
	bool DropSoundtrack();
	bool CarrySomeone();
	bool StopCarrySomeone();
	bool DropSuperHeart(bool Guided);
	bool DropSuperHeartName(const char *DestinationName, bool Guided);
	bool DropSuperHeartRaw(vec2 StartPos, int XHeartDistance, int YHeartDistance, int Owner, const char *OwnerName, int Team, bool Guided);
	bool TickCarrying();
	bool CancelCarrying();

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone;
	int m_TuneZoneOld;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// used for snapping to just update latency if the scoreboard is active
	int m_aCurLatency[MAX_CLIENTS];

	int m_SentSnaps = 0;

	// used for spectator mode
	int m_SpectatorID;

	bool m_IsReady;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastEmoteGlobal;
	int m_LastKill;
	int m_aLastCommands[4];
	int m_LastCommandPos;
	int m_LastWhisperTo;
	int m_LastInvited;

	int m_SendVoteIndex;

	CTeeInfo m_TeeInfos;

	int m_DieTick;
	int m_PreviousDieTick;
	std::optional<int> m_Score;
	int m_JoinTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	int m_TeamChangeTick;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

	// OpenGores
	struct
	{
		// powers
		bool m_HasRainbow;
		bool m_HasRainbowEnabled;

		bool m_HasRainbowBlack;
		bool m_HasRainbowBlackEnabled;

		bool m_HasSplash;
		bool m_HasSplashEnabled;

		bool m_HasExplosion;
		bool m_HasExplosionEnabled;

		bool m_HasSplashPistol;
		bool m_HasSplashPistolEnabled;

		bool m_HasExplosionPistol;
		bool m_HasExplosionPistolEnabled;

		bool m_HasStar;
		bool m_HasStarEnabled;

		bool m_HasAuraDot;
		bool m_HasAuraDotEnabled;

		bool m_HasAuraGun;
		bool m_HasAuraGunEnabled;

		bool m_HasAuraShotgun;
		bool m_HasAuraShotgunEnabled;

		bool m_HasTrail;
		bool m_HasTrailEnabled;
	} m_Powers;

	struct
	{
		bool m_HasEmotion;
		bool m_HasSoundtrack;
		bool m_HasDropHeart;
		bool m_HasDropShield;
		bool m_HasDropNinjaSword;
		bool m_HasGuidedHeart;
		bool m_HasGuidedShield;
		bool m_HasGuidedNinjaSword;
		bool m_HasCarry;
	} m_PowersActivable;

	// extra data for powers
	struct
	{
		int m_RainbowColor;
		int m_RainbowColorLight;
		int m_RainbowColorNumber;

		int m_LastCarryTick;
		int m_CarryTimeRemaining;
		CCharacter *m_CarryCharacter;

		int m_LastEmotionTick;
		int m_LastSoundtrackTick;
		int m_LastDropTick;
		int m_LastGunEffectTick;

		int m_HasStarSpawned;
		int m_HasAuraDotSpawned;
		int m_HasAuraGunSpawned;
		int m_HasAuraShotgunSpawned;
		int m_HasTrailSpawned;
	} m_PowersData;

	// flag system
	int m_ShowFlag;

private:
	const uint32_t m_UniqueClientID;
	CCharacter *m_pCharacter;
	int m_NumInputs;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	bool m_WeakHookSpawn;
	int m_ClientID;
	int m_Team;

	int m_Paused;
	int64_t m_ForcePauseTime;
	int64_t m_LastPause;
	bool m_Afk;

	int m_DefEmote;
	int m_OverrideEmote;
	int m_OverrideEmoteReset;
	bool m_Halloween;

public:
	enum
	{
		PAUSE_NONE = 0,
		PAUSE_PAUSED,
		PAUSE_SPEC
	};

	enum
	{
		TIMERTYPE_DEFAULT = -1,
		TIMERTYPE_GAMETIMER = 0,
		TIMERTYPE_BROADCAST,
		TIMERTYPE_GAMETIMER_AND_BROADCAST,
		TIMERTYPE_SIXUP,
		TIMERTYPE_NONE,
	};

	bool m_DND;
	int64_t m_FirstVoteTick;
	char m_aTimeoutCode[64];

	void ProcessPause();
	int Pause(int State, bool Force);
	int ForcePause(int Time);
	int IsPaused() const;

	bool IsPlaying() const;
	int64_t m_Last_KickVote;
	int64_t m_Last_Team;
	int m_ShowOthers;
	bool m_ShowAll;
	vec2 m_ShowDistance;
	bool m_SpecTeam;
	bool m_NinjaJetpack;

	int m_ChatScore;

	bool m_Moderating;

	void UpdatePlaytime();
	void AfkTimer();
	void SetAfk(bool Afk);
	void SetInitialAfk(bool Afk);
	bool IsAfk() const { return m_Afk; }

	int64_t m_LastPlaytime;
	int64_t m_LastEyeEmote;
	int64_t m_LastBroadcast;
	bool m_LastBroadcastImportance;

	CNetObj_PlayerInput *m_pLastTarget;
	bool m_LastTargetInit;

	bool m_EyeEmoteEnabled;
	int m_TimerType;

	int GetDefaultEmote() const;
	void OverrideDefaultEmote(int Emote, int Tick);
	bool CanOverrideDefaultEmote() const;

	bool m_FirstPacket;
	int64_t m_LastSQLQuery;
	void ProcessScoreResult(CScorePlayerResult &Result);
	std::shared_ptr<CScorePlayerResult> m_ScoreQueryResult;
	std::shared_ptr<CScorePlayerResult> m_ScoreFinishResult;
	bool m_NotEligibleForFinish;
	int64_t m_EligibleForFinishCheck;
	bool m_VotedForPractice;
	int m_SwapTargetsClientID; // Client ID of the swap target for the given player
	bool m_BirthdayAnnounced;

	CSaveTee m_LastTeleTee;
};

#endif
