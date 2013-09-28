/* The MIT License (MIT)

Copyright (c) 2013 Téo .L

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "interface.h"
#include "engine/iserverplugin.h"
#include "eiface.h"
#include "tier1.h"
#include "convar.h"
#include "strtools.h"
#include "game/server/iplayerinfo.h"
#include "engine/IEngineTrace.h"
#include "igameevents.h"
#include "threadtools.h"
#include "usercmd.h"
#include "iclient.h"
#include "inetchannelinfo.h"
#include "inetchannel.h"
#include "edict.h"
#include <string>
#include "server_class.h"
#include "takedamageinfo.h"

class IMoveHelper;
class CCSPlayer;
class CBaseCombatWeapon;
class CBaseCombatCharacter;
class CWeaponCSBase;

#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include <vector>
#include "stdarg.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <signal.h>
#include <sys/mman.h>
#include <assert.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
//#include "tier0/memdbgon.h"

// Interfaces from the engine
IVEngineServer	*engine = NULL;
IEngineTrace *enginetrace = NULL;
IPlayerInfoManager *playerinfomanager = NULL;
IServerGameClients * gameclients = NULL;
IServerGameEnts * gameents = NULL;
IServerGameDLL* gamedll = NULL;

bool alreadyHooked = false;
bool weaponHooked = false;

#define IN_ATTACK		(1 << 0)

#ifdef WIN32
typedef void (__stdcall *PlayerRunCommand_t)(CUserCmd*, IMoveHelper*);
#else
typedef void (*PlayerRunCommand_t)(CCSPlayer *, CUserCmd*, IMoveHelper*);

#endif

typedef void (*RandomSeed_t)(unsigned int);
typedef float (*RandomFloat_t)(float, float);

bool noSpread = false;

inline DWORD GetVFuncAddr( DWORD* classptr, int vtable)
{
	VirtualProtect( &classptr[vtable], sizeof(DWORD), PAGE_EXECUTE_READWRITE, NULL );
	return classptr[vtable];
}

DWORD VirtualTableHook( DWORD* classptr, int vtable, DWORD newInterface )
{
	DWORD dwOld, dwStor = 0x0;
#ifdef WIN32
	VirtualProtect( &classptr[vtable], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwOld );
#else
	mprotect(&classptr[vtable], sizeof(DWORD), PROT_READ | PROT_WRITE | PROT_EXEC );
#endif
	dwStor = GetVFuncAddr(classptr, vtable);
	*(DWORD*)&classptr[vtable] = newInterface;
#ifdef WIN32
	VirtualProtect(&classptr[vtable], sizeof(DWORD), dwOld, &dwOld);
#else
	mprotect(&classptr[vtable], sizeof(DWORD), PROT_READ | PROT_EXEC );
#endif
	return dwStor;
}

#define TRACELEN 8192.0f

#ifdef WIN32
#define VTABLE_CMD 417

#define PUSHAD __asm pushad;
#define POPAD __asm popad;
#else
#define VTABLE_CMD 418

#define PUSHAD asm ( "pusha" );
#define POPAD asm ( "popa" );
#endif

PlayerRunCommand_t gpPlayerRunCommand = NULL;
//FileRequested_t gpFileRequested = NULL;
//FileDenied_t gpFileDenied = NULL;

CCSPlayer * gCSP = NULL; // For Unhook

//float GetSpread(CCSPlayer * cbe);

//float GetCone(CCSPlayer * cbe);

//void UpdateAccuracy(CCSPlayer * cbe);

class CheaterInfo;

CheaterInfo * getCheaterInfoFromBasePlayer(CCSPlayer * player);

class EmptyClass {};

class CheaterInfo
{
public:
	edict_t* cheaterEdict;
	bool isShotOverrided;
	bool fireNextTick;
	CCSPlayer * BasePlayer;
	int waitUntil;
	float *flSpread;
	float *flCone;
	CBaseCombatWeapon* activeweapon;

	CheaterInfo(edict_t *cheater, CCSPlayer * base)
	{
		cheaterEdict = cheater;
		isShotOverrided = false;
		fireNextTick = false;
		BasePlayer = base;
		waitUntil = 0;
		activeweapon = NULL;
	};
	~CheaterInfo(){};
};

//union {datamap_t *(EmptyClass::*GetDataDescMap)() };

//class VFuncs { datamap_t * GetDataDescMap(CBaseCombatWeapon *pThisPtr); };


std::list<CheaterInfo *> cheaters;

CheaterInfo * getCheaterInfoFromBasePlayer(CCSPlayer * player)
{
	for(std::list<CheaterInfo *>::iterator it = cheaters.begin(); it != cheaters.end(); ++it)
	{
		if((*it)->BasePlayer == player) return (*it);
	}
	return NULL;
}

//---------------------------------------------------------------------------------
// Purpose: a sample 3rd party plugin class
//---------------------------------------------------------------------------------
class CEmptyServerPlugin: public IServerPluginCallbacks
{
public:
	CEmptyServerPlugin();
	~CEmptyServerPlugin();

	// IServerPluginCallbacks methods
	virtual bool			Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
	virtual void			Unload( void );
	virtual void			Pause( void );
	virtual void			UnPause( void );
	virtual const char     *GetPluginDescription( void );      
	virtual void			LevelInit( char const *pMapName );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			GameFrame( bool simulating );
	virtual void			LevelShutdown( void );
	virtual void			ClientActive( edict_t *pEntity );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername );
	virtual void			SetCommandClient( int index );
	virtual void			ClientSettingsChanged( edict_t *pEdict );
	virtual PLUGIN_RESULT	ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual PLUGIN_RESULT	ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual PLUGIN_RESULT	NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );

	virtual int GetCommandIndex() { return m_iClientCommandIndex; }
private:
	int m_iClientCommandIndex;
};

#ifdef WIN32
void __stdcall nPlayerRunCommand(CUserCmd* pCmd, IMoveHelper* pMoveHelper)
{
	__asm pushad;

	CCSPlayer * thisptr = NULL;

	// Retrieve "this" pointer.
	__asm mov thisptr, ecx; // Registered in ecx under windows

	if(thisptr)
	{
		CheaterInfo * cheater = getCheaterInfoFromBasePlayer(thisptr);

		if(cheater && pCmd && pMoveHelper)
		{
			int lastGoodIndex = 0;
			float LastDistance = 0.0f;
			for(char index = 1; index <= 64; index++) // Get Best target
			{
				edict_t *MyEdict = engine->PEntityOfEntIndex(index);
				if(MyEdict)
				{
					IPlayerInfo * MyPlayerInfo = playerinfomanager->GetPlayerInfo(MyEdict);
					if(MyPlayerInfo)
					{
						Vector vecLocalPos;
						Vector vecTargetPos;
						gameclients->ClientEarPosition(MyEdict, &vecLocalPos);
						gameclients->ClientEarPosition(MyEdict, &vecTargetPos);
						QAngle myAngles = pCmd->viewangles;

						// Is in FOV

						QAngle angBetweenMeAndTarget;
						Vector vecBetweenMeAndTarget(vecLocalPos - vecTargetPos);
						VectorAngles(vecBetweenMeAndTarget, angBetweenMeAndTarget);
						float distance_x = angBetweenMeAndTarget.x - myAngles.x;
						float distance_y = angBetweenMeAndTarget.y - myAngles.y;

						if(fabs(distance_x) < 45 && fabs(distance_y) < 45)
						{
							Ray_t ray;
							trace_t trace;
							ray.Init(vecLocalPos, vecTargetPos);
							enginetrace->TraceRay(ray,(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER/*|CONTENTS_WINDOW*/|CONTENTS_DEBRIS|CONTENTS_HITBOX),NULL,&trace);

							if (trace.allsolid || trace.DidHitWorld()) continue;

							edict_t* target = gameents->BaseEntityToEdict(trace.m_pEnt);
							if ( target && !engine->IndexOfEdict(target) == 0)
							{
						#undef GetClassName
								if(strcmp(target->GetClassName(), "player") == 0)
								{
									IPlayerInfo* targetinfo = playerinfomanager->GetPlayerInfo(target);
									if(targetinfo)
									{
										int ta = targetinfo->GetTeamIndex();
										int tb = playerinfomanager->GetPlayerInfo(gameents->BaseEntityToEdict((CBaseEntity *)thisptr))->GetTeamIndex();
										if( ta != tb )
										{
											if( targetinfo->IsPlayer() && !targetinfo->IsHLTV() && !targetinfo->IsObserver() && !targetinfo->IsDead() )
											{
												if(LastDistance > (distance_x * distance_y))
												{
													lastGoodIndex = index;
													LastDistance = (distance_x * distance_y);
												}
											}
										} 
									}
								}
							}
						}
					}
				}
			}

			// Process Smooth Aim on Target

			if(lastGoodIndex)
			{

			}
		}
		__asm popad;

		gpPlayerRunCommand(pCmd, pMoveHelper);
		//__asm add esp, 8;
	}
#else
void nPlayerRunCommand(CCSPlayer * thisptr, CUserCmd* pCmd, IMoveHelper* pMoveHelper)
{
	asm ( "pusha" );

	if(thisptr)
	{
		CheaterInfo * cheater = getCheaterInfoFromBasePlayer(thisptr);

		if(cheater && pCmd && pMoveHelper)
		{
			if(cheater->isShotOverrided) // It will be next tick after cheated shot
			{
				pCmd->buttons &= ~IN_ATTACK; 
				pCmd->hasbeenpredicted = true;
				cheater->isShotOverrided = false;
				cheater->waitUntil = pCmd->tick_count + 7; // Wait a little before re-shot. 1 second = 66 ticks. 1 tick = 0.01515 ms.
			}
			if((! pCmd->buttons | IN_ATTACK) && cheater->ShouldFire(pCmd) && (pCmd->tick_count >= cheater->waitUntil)) 
			{
				pCmd->buttons |= IN_ATTACK; // Cheated shot.
				pCmd->hasbeenpredicted = true;
				cheater->isShotOverrided = true;
			}
		}
	}

	asm ( "popa" );

	gpPlayerRunCommand(thisptr, pCmd, pMoveHelper);
}
#endif

// 
// The plugin is a static singleton that is exported as an interface
//
CEmptyServerPlugin g_EmtpyServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EmtpyServerPlugin );

//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CEmptyServerPlugin::CEmptyServerPlugin()
{
	m_iClientCommandIndex = 0;
}

CEmptyServerPlugin::~CEmptyServerPlugin()
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
bool CEmptyServerPlugin::Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	engine            = (IVEngineServer *)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);
	playerinfomanager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER,NULL);
	enginetrace = (IEngineTrace *)interfaceFactory(INTERFACEVERSION_ENGINETRACE_SERVER,NULL);
	gameclients = (IServerGameClients *)gameServerFactory(INTERFACEVERSION_SERVERGAMECLIENTS,NULL);
	gameents = (IServerGameEnts *)gameServerFactory(INTERFACEVERSION_SERVERGAMEENTS,NULL);
	gamedll = (IServerGameDLL *)gameServerFactory(INTERFACEVERSION_SERVERGAMEDLL,NULL);

	if(!engine || !playerinfomanager || !enginetrace || !gameclients || !gameents /*|| !botmanager*/) return false;
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2 );
	return true;
}

PLUGIN_RESULT CEmptyServerPlugin::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	if(pEntity)
	{
		const char * arg = args.Arg(0);
		if(strcmp(arg, "givemepower") == 0) // Kind of password ... DON'T USE THIS COMMAND MORE THAN ONCE OR THE SERVER WILL FREEZE
		{
			CCSPlayer * BasePlayer = NULL;
			BasePlayer = reinterpret_cast<CCSPlayer *>(pEntity->GetUnknown()->GetBaseEntity());
			printf("CCSPlayer : %X\n", BasePlayer);
			if(BasePlayer)
			{
				if(!getCheaterInfoFromBasePlayer(BasePlayer)) // = is not already in list
				{
					CheaterInfo * myCheater = new CheaterInfo(pEntity, BasePlayer);
					if(!alreadyHooked)
					{
						IClient * client = reinterpret_cast<IClient *>((reinterpret_cast<INetChannel *>(engine->GetPlayerNetInfo(engine->IndexOfEdict(pEntity))))->GetMsgHandler());
						DWORD* pdwNewInterface = ( DWORD* )*( DWORD* )BasePlayer;
						*(DWORD*)&(gpPlayerRunCommand) = VirtualTableHook( pdwNewInterface, VTABLE_CMD, ( DWORD )nPlayerRunCommand );
						printf("gpPlayerRunCommand : %X, 4 * VTABLE_CMD + BasePlayer : %X\n", gpPlayerRunCommand, (4*VTABLE_CMD + (DWORD)BasePlayer));
						//*(DWORD*)&(gpWeapon_Switch) = VirtualTableHook( pdwNewInterface, VTABLE_WEAPON, ( DWORD )nWeapon_Switch );

						alreadyHooked = true;
					}

					cheaters.push_back(myCheater);
				}
			}
			return PLUGIN_STOP;
		}
		else if((strcmp(arg, "nospread") == 0) && alreadyHooked == true) noSpread = !noSpread;
	}
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::Unload( void )
{
	//if(alreadyHooked && gCSP && vtable_cmd)
	//{
	//	DWORD* pdwNewInterface = ( DWORD* )*( DWORD* )gCSP;
	//	VirtualTableHook( pdwNewInterface, vtable_cmd, ( DWORD )gpPlayerRunCommand ); // Unhook
	//	}
	//delete[] &cheaters;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::Pause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::UnPause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char *CEmptyServerPlugin::GetPluginDescription( void )
{
	return "Simple Server-Side TriggerBot by JouHn";
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::LevelInit( char const *pMapName )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::GameFrame( bool simulating )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::LevelShutdown( void ) // !!!!this can get called multiple times per map change
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientActive( edict_t *pEntity )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientDisconnect( edict_t *pEntity )
{
	if(pEntity && !pEntity->IsFree())
	{
		IServerUnknown * unkn = pEntity->GetUnknown();
		if(unkn)
		{
			CBaseEntity * cbe = unkn->GetBaseEntity();
			if(cbe)
			{
				CCSPlayer * csp = reinterpret_cast<CCSPlayer *>(cbe);
				if(csp)
				{
					CheaterInfo * myCheater = getCheaterInfoFromBasePlayer(csp);
					if(myCheater)
					{
						cheaters.remove(myCheater);
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------
// Purpose: called on		
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::SetCommandClient( int index )
{
	m_iClientCommandIndex = index;
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientSettingsChanged( edict_t *pEdict )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT CEmptyServerPlugin::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------


//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT CEmptyServerPlugin::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
}
