//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//=============================================================================

#ifndef IOSPLATFORM_H
#define IOSPLATFORM_H

#include "platform.h"
#include <SDL.h>

struct IRadMemoryHeap;
class tPlatform;
class tContext;

class IosPlatform : public Platform
{
public:
    static IosPlatform* CreateInstance();
    static IosPlatform* GetInstance();
    static void DestroyInstance();

    static bool InitializeWindow();
    static void InitializeFoundation();
    static void InitializeMemory();
    static void ShutdownMemory();
    
    static void IOSLog(const char* fmt, ...);
    static void IOSLogPath(const char* label, const char* path);

    virtual void InitializePlatform();
    virtual void ShutdownPlatform();

    virtual void LaunchDashboard();
    virtual void ResetMachine();

    virtual void DisplaySplashScreen( SplashScreen screenID,
        const char* overlayText = NULL,
        float fontScale = 1.0f,
        float textPosX = 0.0f,
        float textPosY = 0.0f,
        tColour textColour = tColour( 255,255,255 ),
        int fadeFrames = 3 );

    virtual void DisplaySplashScreen( const char* textureName,
        const char* overlayText = NULL,
        float fontScale = 1.0f,
        float textPosX = 0.0f,
        float textPosY = 0.0f,
        tColour textColour = tColour( 255,255,255 ),
        int fadeFrames = 3 );

    virtual bool OnDriveError( radFileError error, const char* pDriveName, void* pUserData );
    virtual void OnControllerError( const char* msg );

    SDL_Window* GetWindow() const { return mWnd; }

private:
    IosPlatform();
    virtual ~IosPlatform();

    IosPlatform( const IosPlatform& );
    IosPlatform& operator=( const IosPlatform& );

    virtual void InitializeFoundationDrive();
    virtual void ShutdownFoundation();

    virtual void InitializePure3D();
    virtual void ShutdownPure3D();

    void InitializeContext();

    // Unlike tvOS (no touchscreen, controller mandatory), iOS ships with the
    // touch control HUD (see code/input/touch/) so a physical controller is
    // optional. These just log/track controller presence for UX purposes.
    bool HasAnyController() const;

private:
    static IosPlatform* spInstance;

    static SDL_Window* mWnd;

    tPlatform* mpPlatform;
    tContext* mpContext;
};

#endif
