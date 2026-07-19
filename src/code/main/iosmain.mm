//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//=============================================================================

#include <SDL.h>
#include <SDL_main.h>

#ifdef main
#undef main
#endif

#include <main/game.h>
#include <main/iosplatform.h>
#include <main/singletons.h>
#include <main/commandlineoptions.h>

#include <memory/srrmemory.h>
#include <p3d/entity.hpp>

//========================================
// Forward Declarations
//========================================
static void ProcessCommandLineArguments( int argc, char *argv[] );
static void ProcessCommandLineArgumentsFromFile();

int SDL_main( int argc, char *argv[] );

extern "C" int main( int argc, char *argv[] )
{
    @autoreleasepool
    {
        return SDL_UIKitRunApp( argc, argv, SDL_main );
    }
}

int SDL_main( int argc, char *argv[] )
{
    // Pick out and store command line settings.
    CommandLineOptions::InitDefaults();
    ProcessCommandLineArguments( argc, argv );
    ProcessCommandLineArgumentsFromFile();

    // Same rationale as the tvOS port: use native GameController.framework
    // (via code/input/ios/ios_controller.mm) instead of SDL2's controller
    // subsystem. SDL is only used for window/video/events and, on iOS,
    // touch-finger events consumed by code/input/touch/.
    SDL_SetHint(SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS, "0");

    // Initialize SDL for video and events only - NO controller/joystick subsystems
    SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO );

    IosPlatform::InitializeMemory();

    if( !IosPlatform::InitializeWindow() )
    {
        return 0;
    }

    IosPlatform::InitializeFoundation();

    srand( Game::GetRandomSeed() );

#ifndef RAD_RELEASE
    tName::SetAllocator( GMA_DEBUG );
#endif

    HeapMgr()->PushHeap( GMA_PERSISTENT );

    CreateSingletons();

    IosPlatform* pPlatform = IosPlatform::CreateInstance();
    rAssert( pPlatform != NULL );

    Game* pGame = Game::CreateInstance( pPlatform );
    rAssert( pGame != NULL );

    pGame->Initialize();

    HeapMgr()->PopHeap( GMA_PERSISTENT );

    pGame->Run();

    pGame->Terminate();

    DestroySingletons();

    Game::DestroyInstance();

    pPlatform->ShutdownPlatform();

    IosPlatform::DestroyInstance();

    IosPlatform::ShutdownMemory();

#ifndef RAD_RELEASE
    tName::SetAllocator( RADMEMORY_ALLOC_DEFAULT );
#endif

    SDL_Quit();

    return 0;
}

static void ProcessCommandLineArguments( int /*argc*/, char* /*argv*/[] )
{
}

static void ProcessCommandLineArgumentsFromFile()
{
}
