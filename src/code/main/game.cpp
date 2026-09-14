//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        game.cpp
//
// Description: The game loop
//
// History:     + Stolen and cleaned up from Penthouse -- Darwin Chau
//
//=============================================================================

//========================================
// System Includes
//========================================
// Standard Library
#include <stdlib.h>
#include <string.h>
// Foundation Tech
#include <raddebug.hpp>
#include <raddebugcommunication.hpp>
#include <raddebugconsole.hpp>
#include <raddebugwatch.hpp>
#include <radfile.hpp>
#include <radmemorymonitor.hpp>
#include <radtime.hpp>
// Pure3D
#include <p3d/loadmanager.hpp>
#include <p3d/utility.hpp>

#include <main/platformdef.h>

#ifdef RAD_SDL_PLATFORM
#include <SDL.h>  // for SDL_PollEvent...
#endif

//========================================
// Project Includes
//========================================
#include <contexts/contextenum.h>
#include <debug/profiler.h>
#include <gameflow/gameflow.h>
#include <presentation/gui/ingame/guiscreenmissionload.h>
#include <main/commandlineoptions.h>
#include <main/game.h>
#include <main/platform.h>

#ifdef RAD_GAMECUBE
#include <main/gamecube_extras/gcmanager.h>
#endif

#include <memory/srrmemory.h>
#include <memory/memoryutilities.h>

#include <render/RenderFlow/renderflow.h>
#include <sound/soundmanager.h>
#include <input/inputmanager.h>

#if defined(RAD_ANDROID) || defined(RAD_IOS)
#include <input/touch/touchinputmodemanager.h>
#include <input/touch/touchinputadapter.h>
#include <input/touch/touchcontextresolver.h>
#include <input/touch/touchhudsystem.h>
#include <worldsim/avatarmanager.h>
#include <input/touch/touchassetextractor.h>
#include <input/touch/touchassetmanager.h>
#include <input/touch/touchhudrenderer.h>
#include <input/touch/touchcontrolsconfigurationmanager.h>
#include "iosplatform.h"
#endif

//******************************************************************************
//
// Global Data, Local Data, Local Classes
//
//******************************************************************************

//
// Static pointer to instance of this singleton.
//
Game* Game::spInstance = NULL;

bool g_inDemoMode = false;

#if defined(RAD_ANDROID) || defined(RAD_IOS)

// Ported from Carlox33/The-Simpsons-Hit-and-Run-Android's touch control
// integration (code/main/game.cpp). Drives the touch HUD/context system from
// the same game state both platforms already track.

static bool GetTouchAvatarInVehicleForPlayer0()
{
    /*
     * Keep this helper conservative.
     *
     * It is only queried when the current context is gameplay.
     * AvatarManager is the correct long-term source for detecting whether
     * player 0 is currently inside a vehicle.
     */
    Avatar* avatar = GetAvatarManager()->GetAvatarForPlayer( 0 );

    if ( avatar == NULL )
    {
        return false;
    }

    return avatar->IsInCar();
}

static void UpdateTouchContextResolverFromGame()
{
    if ( GetGameFlow() == NULL || InputManager::GetInstance() == NULL )
    {
        return;
    }

    const int currentContext = GetGameFlow()->GetCurrentContext();
    const unsigned inputGameState = InputManager::GetInstance()->GetGameState();

    bool avatarInVehicle = false;

    /*
     * Only query AvatarManager while actually in gameplay.
     * This avoids touching avatar systems during boot, frontend, loading or exit.
     */
    if ( currentContext == CONTEXT_GAMEPLAY )
    {
        avatarInVehicle = GetTouchAvatarInVehicleForPlayer0();
    }

    TouchContextResolver::GetInstance().UpdateFromGameState(
        currentContext,
        inputGameState,
        avatarInVehicle
    );
}

static void UpdateTouchHudSystemFromSDLEvent( const SDL_Event& msg )
{
#if SDL_MAJOR_VERSION < 3

    switch ( msg.type )
    {
        case SDL_FINGERDOWN:
        {
            TouchInputAdapter::GetInstance().SetEnabled( true );
            TouchInputAdapter::GetInstance().SetTargetControllerIndex( 0 );

            TouchHudSystem::GetInstance().HandleFingerDown(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_FINGERMOTION:
        {
            TouchHudSystem::GetInstance().HandleFingerMove(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_FINGERUP:
        {
            TouchHudSystem::GetInstance().HandleFingerUp(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        default:
        {
            break;
        }
    }

#else

    switch ( msg.type )
    {
        case SDL_EVENT_FINGER_DOWN:
        {
            TouchInputAdapter::GetInstance().SetEnabled( true );
            TouchInputAdapter::GetInstance().SetTargetControllerIndex( 0 );

            TouchHudSystem::GetInstance().HandleFingerDown(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_EVENT_FINGER_MOTION:
        {
            TouchHudSystem::GetInstance().HandleFingerMove(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_EVENT_FINGER_UP:
        {
            TouchHudSystem::GetInstance().HandleFingerUp(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        default:
        {
            break;
        }
    }

#endif
}

static void UpdateTouchInputModeFromSDLEvent( const SDL_Event& msg )
{
#if SDL_MAJOR_VERSION < 3

    switch ( msg.type )
    {
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
        {
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            break;
        }

        case SDL_CONTROLLERDEVICEADDED:
        case SDL_JOYDEVICEADDED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadConnected();
            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_JOYDEVICEREMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_JOYBUTTONDOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_CONTROLLERAXISMOTION:
        {
            if ( msg.caxis.value > 8000 || msg.caxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        case SDL_JOYAXISMOTION:
        {
            if ( msg.jaxis.value > 8000 || msg.jaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        default:
        {
            break;
        }
    }

#else

    switch ( msg.type )
    {
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        {
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            break;
        }

        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_JOYSTICK_ADDED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadConnected();
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            if ( msg.gaxis.value > 8000 || msg.gaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        {
            if ( msg.jaxis.value > 8000 || msg.jaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        default:
        {
            break;
        }
    }

#endif
}

#endif // RAD_ANDROID || RAD_IOS

//#define DEMO_MODE_PROFILER

#ifdef DEMO_MODE_PROFILER
    #define DEMOPROFILE(X) X
#else
    #define DEMOPROFILE(X)
#endif

#ifdef DEMO_MODE_PROFILER

#ifdef RAD_PS2
#include <libgraph.h>
#endif

DemoProfiler::DemoProfiler(unsigned mf) :
    recording(false), maxFrames(mf), nChannel(0), currentFrame(0),
    alertStatus(PROFILER_ALERT_GREEN),
    numFramesBelow_20(0), numFramesBetween_20_30(0),numFramesBetween_30_40(0), numFramesAbove_40(0)
{
    for( int i=0; i < MAX_CHANNEL; i++)
    {
        channel[i] = NULL;
    }
}
    
void DemoProfiler::AddChannel(unsigned c, const char* name)
{
    rReleaseAssert(c < MAX_CHANNEL);
    channel[c] = new Channel;
    channel[c]->samples = new unsigned[maxFrames];
    memset(channel[c]->samples, 0, maxFrames*sizeof(unsigned));
    channel[c]->t0 = 0;
    strncpy(channel[c]->name, name, 254);
    nChannel++;
}

void DemoProfiler::Start(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->t0 = radTimeGetMicroseconds64();
    }
}


void DemoProfiler::Stop(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        radTime64 elapsed = radTimeGetMicroseconds64() - channel[c]->t0;
        channel[c]->samples[currentFrame] += elapsed;

        if(c == 0)
        {
            alertStatus = PROFILER_ALERT_GREEN;
            if( elapsed >= 50000 )
            {
                numFramesBelow_20++;
                alertStatus = PROFILER_ALERT_RED;
            }
            else if( (elapsed < 50000) && (elapsed >= 33333) )
            {
                numFramesBetween_20_30++;
                alertStatus = PROFILER_ALERT_YELLOW;
            }
            else if( (elapsed < 33333) && (elapsed >= 25000) )
            {
                numFramesBetween_30_40++;
            }
            else
            {
                numFramesAbove_40++;
            }
        }
    }
}

void DemoProfiler::Set(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] = val;
    }
}

void DemoProfiler::StartRecording()
{
    recording = true;
}

bool DemoProfiler::IsRecording()
{
    return recording;
}

unsigned DemoProfiler::GetSample(unsigned c)
{
    return (recording && channel[c]) ? channel[c]->samples[currentFrame] : 0;
}

unsigned DemoProfiler::GetCurrentFrame()
{
    return currentFrame;
}

void DemoProfiler::Accumulate(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] += val;
    }
}

void DemoProfiler::NextFrame()
{
    if( recording )
    {
        currentFrame++;
        if(currentFrame >= maxFrames)
        {
            Dump();
            recording = false;
        }
    }
}

DemoProfiler::AlertStatus DemoProfiler::GetAlertStatus()
{
    return alertStatus;
}

void DemoProfiler::Dump()
{
    rReleasePrintf("\n\n~~~~~~~~~~~~~~~~~~~~~ PROFILER STATS ~~~~~~~~~~~~~~~~~~~~~\n");
    rReleasePrintf(    "Total Frames: %d\n"
                       "< 20 fps: %d (%.2f%%)\n"
                       "20-30 fps: %d (%.2f%%)\n"
                       "30-40 fps: %d (%.2f%%)\n"
                       ">40 fps: %d (%.2f%%)\n",
                       maxFrames,
                       numFramesBelow_20, 100.0f * (float)numFramesBelow_20 / (float)maxFrames,
                       numFramesBetween_20_30, 100.0f * (float)numFramesBetween_20_30 / (float)maxFrames,
                       numFramesBetween_30_40, 100.0f * (float)numFramesBetween_30_40 / (float)maxFrames,
                       numFramesAbove_40, 100.0f * (float)numFramesAbove_40 / (float)maxFrames );

    for( unsigned i=0; i < MAX_CHANNEL; i++)
    {
        if(channel[i])
        {
            rReleasePrintf("%s\t", channel[i]->name);
        }
    }
    rReleasePrintf("\n");

    for( unsigned i=0; i < maxFrames; i++ )
    {
        for( unsigned j=0; j < MAX_CHANNEL; j++ )
        {
            if(channel[j])
            {
                rReleasePrintf("%.1f\t", (float)channel[j]->samples[i] * 0.001f);
            }
        }
        rReleasePrintf("\n");

        if( !(i % 10) )
        {
            rmt::Sin(0.0f);
#ifdef RAD_PS2
            sceGsSyncV(0);
#endif
        }
    }

    while(1)
    {
        rmt::Sin(0.0f);
    }
}


DemoProfiler g_DemoProfiler( 1850 );  // about a minute at 30 fps

#include <pddi/pddi.hpp>

static bool g_DemoProfiler_Started = false;
static const int g_DemoProfiler_StartFrame = 150;
static int g_DemoProfiler_CurrentFrame = 0;

#endif


//******************************************************************************
//
// Public Member Functions
//
//******************************************************************************

//==============================================================================
// Game::CreateInstance
//==============================================================================
// Description: Create the game
//
// Parameters:	platform - the platform that the game is to be created on
//
// Return:      pointer to the created game
//
// Constraints: This is a singleton so only one instance is allowed.
//
//==============================================================================
Game* Game::CreateInstance( Platform* platform )
{
    rAssert( platform != NULL );

    rAssertMsg( (spInstance == NULL), "Trying to create more than one instance of the game!" );

MEMTRACK_PUSH_GROUP( "Game" );
    if( spInstance == NULL )
    {
        spInstance = new(GMA_PERSISTENT) Game( platform );
        rAssert( spInstance != NULL );
    }
MEMTRACK_POP_GROUP( "Game" );
    
    return spInstance;
}


//==============================================================================
// Game::DestroyInstance
//==============================================================================
// Description: Destroy the game
//
// Parameters:	None.
//
// Return:      None.
//
//==============================================================================
void Game::DestroyInstance()
{
    delete( GMA_PERSISTENT, spInstance );
    spInstance = NULL;
}


//==============================================================================
// Game::GetInstance
//==============================================================================
// Synopsis:    Get an instance of the game
//
// Parameters:  None.
//
// Returns:     a poitner to the game
//
// Constraints: Game must be created before this is called
//
//==============================================================================
Game* Game::GetInstance()
{
    rAssertMsg((spInstance != NULL), "Trying to get an instance of the game before it is created!");
    
    return spInstance;
}

//=============================================================================
// Game::GetPlatform
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      Platform
//
//=============================================================================
Platform* Game::GetPlatform()
{
    return mpPlatform;
}


//==============================================================================
// Game::Initialize
//==============================================================================
// Synopsis:    Initialize the game
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Initialize()
{
    rAssert( mpPlatform != NULL );

    //
    // Initialize the platform and core systems.
    //
    mpPlatform->InitializePlatform();

#if defined(RAD_ANDROID) || defined(RAD_IOS)
    // Extract/verify the bundled touch control icons are readable as regular
    // files (Android: unpacked from the APK; iOS: already flat in the app
    // bundle, so this is close to a no-op there), load them via pure3d, load
    // any saved per-player layout, then start the HUD render system.
    TouchAssetExtractor::GetInstance().EnsureAssetsExtracted();
    TouchAssetManager::GetInstance().Initialize();
    TouchControlsConfigurationManager::GetInstance().Initialize();

    TouchHudRenderer::GetInstance().Initialize();
#endif

    //
    // Initialize the sound manager.
    //
    SoundManager::GetInstance()->Initialize();
    
    //
    // Initialize the timer system
    //
    ::radTimeCreateList( &mpTimerList,
                         16, // Default
                         GMA_PERSISTENT );

    rAssert( mpTimerList != NULL );

    //
    // Create the GameFlow & Couple the RenderFlow
    //
    mpGameFlow = GameFlow::CreateInstance();
    mpRenderFlow = RenderFlow::GetInstance();
    mpRenderFlow->DoAllRegistration();

    CGuiScreenMissionLoad::InitializePermanentVariables();

#ifdef RAD_E3
    rReleasePrintf( "\n----------=[  SIMPSONS HIT & RUN - E3 BUILD  ]=----------\n\n" );
#endif

    //
    // Set the starting context
    //
    mpGameFlow->SetContext( CONTEXT_BOOTUP );
}


//==============================================================================
// Game::Terminate
//==============================================================================
// Synopsis:    Clean up and shut down.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Terminate() 
{
    rAssert( mpGameFlow != NULL );
    rAssert( mpRenderFlow != NULL );
    rAssert( mpTimerList != NULL );
    rAssert( mpPlatform != NULL );

#if defined(RAD_ANDROID) || defined(RAD_IOS)
    // Shut down the touch HUD render system before releasing its icon assets.
    TouchHudRenderer::GetInstance().Shutdown();
    TouchAssetManager::GetInstance().Shutdown();
#endif

    //
    // Kill the flow servers.
    //
    mpGameFlow->DestroyInstance();
    mpGameFlow = NULL;
    
    // Render flow destroyed by singletons.cpp
    //mpRenderFlow->DestroyInstance();
    mpRenderFlow = NULL;
    
    //
    // Release the game's references to the timer list.
    //
    mpTimerList->Release();
    mpTimerList = NULL;
}


//==============================================================================
// Game::Run
//==============================================================================
// Synopsis:    This is where game loop spins.  It exits after Stop() is called.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
const unsigned PROFILE_CHANNEL_ALL = 0;
const unsigned PROFILE_CHANNEL_AI = 1;
const unsigned PROFILE_CHANNEL_RENDER = 2;
const unsigned PROFILE_CHANNEL_LOAD = 3;

void Game::Run()
{
    unsigned time = radTimeGetMilliseconds();
    unsigned debugFrame = 0;

    IOSLog("RUN START time=%u", time);

    while (!mExitNow)
    {
        const unsigned newTime = radTimeGetMilliseconds();
        const unsigned elapsed = newTime - time;
        time = newTime;

        IOSLog(
            "F%u BEGIN dt=%u ctx=%d",
            debugFrame,
            elapsed,
            (int)mpGameFlow->GetCurrentContext()
        );

#if defined(RAD_ANDROID) || defined(RAD_IOS)

        IOSLog("F%u TOUCH begin", debugFrame);

        UpdateTouchContextResolverFromGame();
        TouchHudSystem::GetInstance().Update(elapsed);
        TouchInputModeManager::GetInstance().Update(elapsed);

        IOSLog("F%u TOUCH end", debugFrame);

#endif

#ifdef RAD_SDL_PLATFORM

        SDL_Event msg;
        unsigned eventCount = 0;

        while (SDL_PollEvent(&msg))
        {
            ++eventCount;

#if defined(RAD_ANDROID) || defined(RAD_IOS)
            UpdateTouchInputModeFromSDLEvent(msg);
            UpdateTouchHudSystemFromSDLEvent(msg);
#endif

            if (msg.type == SDL_QUIT)
            {
                IOSLog(
                    "F%u SDL_QUIT ctx=%d",
                    debugFrame,
                    (int)GetGameFlow()->GetCurrentContext()
                );

                if (GetGameFlow()->GetCurrentContext() != CONTEXT_FRONTEND &&
                    GetGameFlow()->GetCurrentContext() != CONTEXT_GAMEPLAY &&
                    GetGameFlow()->GetCurrentContext() != CONTEXT_PAUSE)
                {
                    IOSLog("F%u LaunchDashboard", debugFrame);

                    GetGame()->GetPlatform()->LaunchDashboard();

                    IOSLog("F%u Run returning", debugFrame);
                    return;
                }
                else
                {
                    IOSLog("F%u SetContext EXIT", debugFrame);
                    mpGameFlow->SetContext(CONTEXT_EXIT);
                }
            }
        }

        IOSLog(
            "F%u SDL events=%u",
            debugFrame,
            eventCount
        );

#endif

        const bool paused = mpPlatform->PausedForErrors();

        IOSLog(
            "F%u FLOW begin paused=%d ctx=%d",
            debugFrame,
            paused ? 1 : 0,
            (int)mpGameFlow->GetCurrentContext()
        );

        if (!paused)
        {
            IOSLog("F%u TimerList begin", debugFrame);

            mpTimerList->Service();

            IOSLog("F%u TimerList end", debugFrame);

            IOSLog(
                "F%u GameFlow begin ctx=%d",
                debugFrame,
                (int)mpGameFlow->GetCurrentContext()
            );

            mpGameFlow->OnTimerDone(elapsed, NULL);

            IOSLog(
                "F%u GameFlow end ctx=%d exit=%d",
                debugFrame,
                (int)mpGameFlow->GetCurrentContext(),
                mExitNow ? 1 : 0
            );

            if (!mExitNow)
            {
                IOSLog("F%u RenderFlow begin", debugFrame);

                mpRenderFlow->OnTimerDone(elapsed, NULL);

                IOSLog("F%u RenderFlow end", debugFrame);
            }
        }
        else if (mpPlatform->IsControllerError())
        {
            IOSLog("F%u controller-error path", debugFrame);

            if (InputManager::GetInstance())
            {
                IOSLog("F%u InputManager begin", debugFrame);

                InputManager::GetInstance()->Update(elapsed);

                IOSLog("F%u InputManager end", debugFrame);
            }
        }
        else
        {
            IOSLog("F%u generic paused/error path", debugFrame);

#ifdef RAD_GAMECUBE
            GCManager::GetInstance()->OnTimerDone(elapsed, NULL);
#endif
        }

        IOSLog("F%u FileService begin", debugFrame);

        ::radFileService();

        IOSLog("F%u FileService end", debugFrame);

#ifndef RAD_TVOS

        IOSLog("F%u DbgCom begin", debugFrame);

        ::radDbgComService();

        IOSLog("F%u DbgCom end", debugFrame);

        IOSLog("F%u DebugConsole begin", debugFrame);

        ::radDebugConsoleService();

        IOSLog("F%u DebugConsole end", debugFrame);

#endif

        if (CommandLineOptions::Get(CLO_MEMORY_MONITOR))
        {
            IOSLog("F%u MemoryMonitor begin", debugFrame);

            ::radMemoryMonitorService();

            IOSLog("F%u MemoryMonitor end", debugFrame);
        }

        IOSLog("F%u Sound begin", debugFrame);

        SoundManager::GetInstance()->Update();

        IOSLog("F%u Sound end", debugFrame);

        if (mpPlatform->PausedForErrors())
        {
            IOSLog("F%u Sound error-frame begin", debugFrame);

            SoundManager::GetInstance()->UpdateOncePerFrame(
                0,
                NUM_CONTEXTS,
                false,
                true
            );

            IOSLog("F%u Sound error-frame end", debugFrame);
        }

        IOSLog("F%u LoadManager begin", debugFrame);

        p3d::loadManager->SwitchTask();

        IOSLog("F%u LoadManager end", debugFrame);

        ++mFrameCount;

        IOSLog(
            "F%u END engineFrame=%u ctx=%d",
            debugFrame,
            mFrameCount,
            (int)mpGameFlow->GetCurrentContext()
        );

        ++debugFrame;
    }

    IOSLog(
        "RUN EXIT frames=%u engineFrame=%u",
        debugFrame,
        mFrameCount
    );
}

//==============================================================================
// Game::Stop
//==============================================================================
// Synopsis:    Sets the flag to break us out of the game loop.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Stop() 
{
    //
    // Stop any further rendering from happening.
    //
    mExitNow = true;
}


unsigned Game::GetRandomSeed ()
{
    radDate date;
    ::radTimeGetDate (&date);
    return ( ( date.m_Year << 16 ) | (  date.m_Month << 8 ) | ( date.m_Day ) ) ^ ( ( date.m_Second << 24 ) | ( date.m_Minute << 8 ) | ( date.m_Hour ) );
}



//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

//==============================================================================
// Game::Game
//==============================================================================
// Synopsis:    Constructor.
//
// Parameters:  platform - platform on which game should be created
//
// Returns:     N/A.
//
//==============================================================================
Game::Game( Platform* platform ) :
    mpPlatform( platform ),
    mpTimerList( NULL ),
    mpGameFlow( NULL ),
    mpRenderFlow( NULL ),
    mFrameCount( 0 ),
    mExitNow( false ),
    mDemoCount( 0 ),
    mTimeMS( 0 )
{
}


//==============================================================================
// Game::~Game
//==============================================================================
// Synopsis:    Destructor.
//
// Parameters:  None.
//
// Returns:     N/A.
//
//==============================================================================
Game::~Game()
{
}
