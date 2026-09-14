//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//=============================================================================

#include <SDL.h>
#include <SDL_main.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

#include <stdarg.h>
#include <stdio.h>

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

static NSMutableString* gDebugLog = nil;
static UITextView* gDebugView = nil;
static UIWindow* gDebugWindow = nil;

static UIWindowScene* GetForegroundWindowScene()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
    {
        if (scene.activationState != UISceneActivationStateForegroundActive)
            continue;

        if ([scene isKindOfClass:[UIWindowScene class]])
            return (UIWindowScene*)scene;
    }

    return nil;
}

static void IOSCreateDebugOverlay()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (gDebugWindow != nil)
            return;

        UIWindowScene* scene = GetForegroundWindowScene();
        if (scene == nil)
            return;

        gDebugWindow =
            [[UIWindow alloc] initWithWindowScene:scene];

        gDebugWindow.frame = scene.coordinateSpace.bounds;
        gDebugWindow.windowLevel = UIWindowLevelAlert + 1000.0;

        UIViewController* controller =
            [[UIViewController alloc] init];

        controller.view.backgroundColor =
            [UIColor clearColor];

        gDebugWindow.rootViewController = controller;

        gDebugView =
            [[UITextView alloc] initWithFrame:controller.view.bounds];

        gDebugView.autoresizingMask =
            UIViewAutoresizingFlexibleWidth |
            UIViewAutoresizingFlexibleHeight;

        gDebugView.backgroundColor =
            [UIColor colorWithWhite:0.0 alpha:0.72];

        gDebugView.textColor = UIColor.greenColor;

        gDebugView.font =
            [UIFont monospacedSystemFontOfSize:10.0
                                       weight:UIFontWeightRegular];

        gDebugView.editable = NO;
        gDebugView.selectable = YES;

        gDebugView.text = gDebugLog ?: @"";

        [controller.view addSubview:gDebugView];

        gDebugWindow.hidden = NO;
    });
}

static CFTimeInterval gLastDebugRefresh = 0.0;

void IOSLog(const char* fmt, ...)
{
    char buffer[4096];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    NSLog(@"[SHR] %s", buffer);

    @autoreleasepool
    {
        if (gDebugLog == nil)
            gDebugLog = [[NSMutableString alloc] init];

        NSString* line =
            [NSString stringWithFormat:@"%s\n", buffer];

        [gDebugLog appendString:line];

        // Don't allow the in-memory display buffer to grow forever.
        const NSUInteger maxLength = 30000;

        if (gDebugLog.length > maxLength)
        {
            [gDebugLog deleteCharactersInRange:
                NSMakeRange(0, gDebugLog.length - maxLength)];
        }

        CFTimeInterval now = CACurrentMediaTime();

        if ((now - gLastDebugRefresh) < 0.05)
            return;

        gLastDebugRefresh = now;

        NSString* snapshot = [gDebugLog copy];

        dispatch_async(dispatch_get_main_queue(), ^{
            if (gDebugView == nil)
                return;

            gDebugView.text = snapshot;

            NSRange bottom =
                NSMakeRange(gDebugView.text.length, 0);

            [gDebugView scrollRangeToVisible:bottom];
        });
    }
}

int SDL_main( int argc, char *argv[] );

extern "C" int main( int argc, char *argv[] )
{
    @autoreleasepool
    {
        IOSLog("========== SH&R PROCESS START ==========");
        IOSLog("main(): argc=%d", argc);
        IOSLog("main(): before SDL_UIKitRunApp");

        int result = SDL_UIKitRunApp( argc, argv, SDL_main );

        IOSLog("main(): SDL_UIKitRunApp returned %d", result);
        IOSLog("========== SH&R PROCESS END ==========");

        return result;
    }
}

int SDL_main( int argc, char *argv[] )
{
    IOSLog("01 SDL_main entered");

    // ---------------------------------------------------------
    // Verify exactly where the application bundle/assets are.
    // ---------------------------------------------------------
    @autoreleasepool
    {
        NSString* bundlePath = [[NSBundle mainBundle] bundlePath];

        IOSLog(
            "02 bundle path = %s",
            bundlePath != nil ? [bundlePath UTF8String] : "<null>"
        );

        NSString* assetRoot =
            [bundlePath stringByAppendingPathComponent:@"Assets/TheSimpsons"];

        BOOL isDirectory = NO;

        BOOL assetRootExists =
            [[NSFileManager defaultManager]
                fileExistsAtPath:assetRoot
                     isDirectory:&isDirectory];

        IOSLog(
            "03 asset root = %s | exists=%d | directory=%d",
            [assetRoot UTF8String],
            assetRootExists ? 1 : 0,
            isDirectory ? 1 : 0
        );

        NSArray<NSString*>* probes = @[
            @"dialog.rcf",
            @"scripts.rcf",
            @"soundfx.rcf",
            @"art",
            @"scripts",
            @"movies",
            @"sound"
        ];

        for (NSString* item in probes)
        {
            NSString* path =
                [assetRoot stringByAppendingPathComponent:item];

            BOOL probeIsDirectory = NO;

            BOOL exists =
                [[NSFileManager defaultManager]
                    fileExistsAtPath:path
                         isDirectory:&probeIsDirectory];

            IOSLog(
                "ASSET PROBE: %s | exists=%d | directory=%d",
                [item UTF8String],
                exists ? 1 : 0,
                probeIsDirectory ? 1 : 0
            );
        }
    }

    IOSLog("04 initializing command-line defaults");

    CommandLineOptions::InitDefaults();

    IOSLog("05 processing command-line arguments");

    ProcessCommandLineArguments( argc, argv );

    IOSLog("06 processing command-line file");

    ProcessCommandLineArgumentsFromFile();

    // Same rationale as the tvOS port: use native GameController.framework
    // instead of SDL2's controller/joystick subsystem.
    IOSLog("07 setting SDL hints");

    SDL_SetHint(
        SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,
        "0"
    );

    IOSLog("08 before SDL_Init");

    int sdlResult =
        SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO );

    IOSLog(
        "09 SDL_Init returned %d | SDL error = %s",
        sdlResult,
        SDL_GetError()
    );

    if( sdlResult != 0 )
    {
        IOSLog("FATAL: SDL_Init failed");
        return 0;
    }

    IOSLog("10 before IosPlatform::InitializeMemory");

    IosPlatform::InitializeMemory();

    IOSLog("11 InitializeMemory complete");
    IOSLog("12 before IosPlatform::InitializeWindow");

    if( !IosPlatform::InitializeWindow() )
    {
        IOSLog(
            "FATAL: InitializeWindow failed | SDL error = %s",
            SDL_GetError()
        );

        return 0;
    }

    IOSLog("13 InitializeWindow complete");

		IOSCreateDebugOverlay();

		IOSLog("13.1 debug overlay created");
    IOSLog("14 before IosPlatform::InitializeFoundation");

    IosPlatform::InitializeFoundation();

    IOSLog("15 InitializeFoundation complete");

    unsigned int randomSeed = Game::GetRandomSeed();

    IOSLog(
        "16 Game::GetRandomSeed = %u",
        randomSeed
    );

    srand( randomSeed );

#ifndef RAD_RELEASE
    IOSLog("17 setting debug tName allocator");

    tName::SetAllocator( GMA_DEBUG );
#endif

    IOSLog("18 pushing GMA_PERSISTENT heap");

    HeapMgr()->PushHeap( GMA_PERSISTENT );

    IOSLog("19 before CreateSingletons");

    CreateSingletons();

    IOSLog("20 CreateSingletons complete");
    IOSLog("21 before IosPlatform::CreateInstance");

    IosPlatform* pPlatform =
        IosPlatform::CreateInstance();

    IOSLog(
        "22 IosPlatform instance = %p",
        pPlatform
    );

    rAssert( pPlatform != NULL );

    if( pPlatform == NULL )
    {
        IOSLog("FATAL: IosPlatform::CreateInstance returned NULL");
        return 0;
    }

    IOSLog("23 before Game::CreateInstance");

    Game* pGame =
        Game::CreateInstance( pPlatform );

    IOSLog(
        "24 Game instance = %p",
        pGame
    );

    rAssert( pGame != NULL );

    if( pGame == NULL )
    {
        IOSLog("FATAL: Game::CreateInstance returned NULL");
        return 0;
    }

    //
    // THIS IS A VERY IMPORTANT BOUNDARY.
    //
    IOSLog("25 >>> BEFORE Game::Initialize");

    pGame->Initialize();

    IOSLog("26 <<< Game::Initialize RETURNED");

    IOSLog("27 popping GMA_PERSISTENT heap");

    HeapMgr()->PopHeap( GMA_PERSISTENT );

    //
    // If you get this log and then the screen stays black,
    // we're inside Game::Run() and iosmain is no longer
    // where the failure is.
    //
    IOSLog("28 >>> ENTERING Game::Run");

    pGame->Run();

    IOSLog("29 <<< Game::Run RETURNED");

    IOSLog("30 before Game::Terminate");

    pGame->Terminate();

    IOSLog("31 Game::Terminate complete");

    IOSLog("32 before DestroySingletons");

    DestroySingletons();

    IOSLog("33 DestroySingletons complete");

    Game::DestroyInstance();

    IOSLog("34 Game instance destroyed");

    pPlatform->ShutdownPlatform();

    IOSLog("35 platform shutdown complete");

    IosPlatform::DestroyInstance();

    IOSLog("36 platform instance destroyed");

    IosPlatform::ShutdownMemory();

    IOSLog("37 memory shutdown complete");

#ifndef RAD_RELEASE
    tName::SetAllocator( RADMEMORY_ALLOC_DEFAULT );
#endif

    IOSLog("38 before SDL_Quit");

    SDL_Quit();

    IOSLog("39 SDL_main finished normally");

    return 0;
}

static void ProcessCommandLineArguments( int /*argc*/, char* /*argv*/[] )
{
}

static void ProcessCommandLineArgumentsFromFile()
{
}
