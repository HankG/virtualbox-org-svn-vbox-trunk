/* $Id: $ */
/** @file
 * VBoxSeamless - Display notifications.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include "VBoxTray.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <VBox/VMMDev.h>
#include <iprt/assert.h>
#include "helpers.h"
#include <malloc.h>

typedef struct _VBOXDISPLAYCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    /* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
    LONG (WINAPI * pfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);

    /* EnumDisplayDevices does not exist in NT. isVBoxDisplayDriverActive et al. are using these functions. */
    BOOL (WINAPI * pfnEnumDisplayDevices)(IN LPCSTR lpDevice, IN DWORD iDevNum, OUT PDISPLAY_DEVICEA lpDisplayDevice, IN DWORD dwFlags);

} VBOXDISPLAYCONTEXT;

static VBOXDISPLAYCONTEXT gCtx = {0};

int VBoxDisplayInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);

    HMODULE hUser = GetModuleHandle("USER32");

    gCtx.pEnv = pEnv;

    if (NULL == hUser)
    {
        Log(("VBoxTray: Could not get module handle of USER32.DLL!\n"));
        return VERR_NOT_IMPLEMENTED;
    }
    else if (OSinfo.dwMajorVersion >= 5)        /* APIs available only on W2K and up! */
    {
        *(uintptr_t *)&gCtx.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
        Log(("VBoxTray: pfnChangeDisplaySettingsEx = %p\n", gCtx.pfnChangeDisplaySettingsEx));

        *(uintptr_t *)&gCtx.pfnEnumDisplayDevices = (uintptr_t)GetProcAddress(hUser, "EnumDisplayDevicesA");
        Log(("VBoxTray: pfnEnumDisplayDevices = %p\n", gCtx.pfnEnumDisplayDevices));
    }
    else if (OSinfo.dwMajorVersion <= 4)            /* Windows NT 4.0 */
    {
        /* Nothing to do here yet */
    }
    else                                /* Unsupported platform */
    {
        Log(("VBoxTray: Warning, display for platform not handled yet!\n"));
        return VERR_NOT_IMPLEMENTED;
    }

    Log(("VBoxTray: Display init successful.\n"));

    *pfStartThread = true;
    *ppInstance = (void *)&gCtx;
    return VINF_SUCCESS;
}

void VBoxDisplayDestroy (const VBOXSERVICEENV *pEnv, void *pInstance)
{
    return;
}

static bool isVBoxDisplayDriverActive (VBOXDISPLAYCONTEXT *pCtx)
{
    bool result = false;

    if( pCtx->pfnEnumDisplayDevices )
    {
        INT devNum = 0;
        DISPLAY_DEVICE dispDevice;
        FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);
        dispDevice.cb = sizeof(DISPLAY_DEVICE);

        Log(("Checking for active VBox display driver (W2K+)...\n"));

        while (EnumDisplayDevices(NULL,
                                  devNum,
                                  &dispDevice,
                                  0))
        {
            Log(("DevNum:%d\nName:%s\nString:%s\nID:%s\nKey:%s\nFlags=%08X\n\n",
                          devNum,
                          &dispDevice.DeviceName[0],
                          &dispDevice.DeviceString[0],
                          &dispDevice.DeviceID[0],
                          &dispDevice.DeviceKey[0],
                          dispDevice.StateFlags));

            if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                Log(("Primary device.\n"));

                if (strcmp(&dispDevice.DeviceString[0], "VirtualBox Graphics Adapter") == 0)
                    result = true;

                break;
            }

            FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);

            dispDevice.cb = sizeof(DISPLAY_DEVICE);

            devNum++;
        }
    }
    else    /* This must be NT 4 or something really old, so don't use EnumDisplayDevices() here  ... */
    {
        Log(("Checking for active VBox display driver (NT or older)...\n"));

        DEVMODE tempDevMode;
        ZeroMemory (&tempDevMode, sizeof (tempDevMode));
        tempDevMode.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &tempDevMode);     /* Get current display device settings */

        /* Check for the short name, because all long stuff would be truncated */
        if (strcmp((char*)&tempDevMode.dmDeviceName[0], "VBoxDisp") == 0)
            result = true;
    }

    return result;
}

/* Returns TRUE to try again. */
static BOOL ResizeDisplayDevice(ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    BOOL fModeReset = (Width == 0 && Height == 0 && BitsPerPixel == 0);

    DISPLAY_DEVICE DisplayDevice;

    ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
    DisplayDevice.cb = sizeof(DisplayDevice);

    /* Find out how many display devices the system has */
    DWORD NumDevices = 0;
    DWORD i = 0;
    while (EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
    {
        Log(("[%d] %s\n", i, DisplayDevice.DeviceName));

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("ResizeDisplayDevice: Found primary device. err %d\n", GetLastError ()));
            NumDevices++;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("ResizeDisplayDevice: Found secondary device. err %d\n", GetLastError ()));
            NumDevices++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
        DisplayDevice.cb = sizeof(DisplayDevice);
        i++;
    }

    Log(("ResizeDisplayDevice: Found total %d devices. err %d\n", NumDevices, GetLastError ()));

    if (NumDevices == 0 || Id >= NumDevices)
    {
        Log(("ResizeDisplayDevice: Requested identifier %d is invalid. err %d\n", Id, GetLastError ()));
        return FALSE;
    }

    DISPLAY_DEVICE *paDisplayDevices = (DISPLAY_DEVICE *)alloca (sizeof (DISPLAY_DEVICE) * NumDevices);
    DEVMODE *paDeviceModes = (DEVMODE *)alloca (sizeof (DEVMODE) * NumDevices);
    RECTL *paRects = (RECTL *)alloca (sizeof (RECTL) * NumDevices);

    /* Fetch information about current devices and modes. */
    DWORD DevNum = 0;
    DWORD DevPrimaryNum = 0;

    ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
    DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

    i = 0;
    while (EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
    {
        Log(("ResizeDisplayDevice: [%d(%d)] %s\n", i, DevNum, DisplayDevice.DeviceName));

        BOOL bFetchDevice = FALSE;

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("ResizeDisplayDevice: Found primary device. err %d\n", GetLastError ()));
            DevPrimaryNum = DevNum;
            bFetchDevice = TRUE;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("ResizeDisplayDevice: Found secondary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }

        if (bFetchDevice)
        {
            if (DevNum >= NumDevices)
            {
                Log(("ResizeDisplayDevice: %d >= %d\n", NumDevices, DevNum));
                return FALSE;
            }

            paDisplayDevices[DevNum] = DisplayDevice;

            /* First try to get the video mode stored in registry (ENUM_REGISTRY_SETTINGS).
             * A secondary display could be not active at the moment and would not have
             * a current video mode (ENUM_CURRENT_SETTINGS).
             */
            ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
            paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_REGISTRY_SETTINGS, &paDeviceModes[DevNum]))
            {
                Log(("ResizeDisplayDevice: EnumDisplaySettings err %d\n", GetLastError ()));
                return FALSE;
            }

            if (   paDeviceModes[DevNum].dmPelsWidth == 0
                || paDeviceModes[DevNum].dmPelsHeight == 0)
            {
                /* No ENUM_REGISTRY_SETTINGS yet. Seen on Vista after installation.
                 * Get the current video mode then.
                 */
                ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
                paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
                if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                     ENUM_CURRENT_SETTINGS, &paDeviceModes[DevNum]))
                {
                    /* ENUM_CURRENT_SETTINGS returns FALSE when the display is not active:
                     * for example a disabled secondary display.
                     * Do not return here, ignore the error and set the display info to 0x0x0.
                     */
                    Log(("EnumDisplaySettings(ENUM_CURRENT_SETTINGS) err %d\n", GetLastError ()));
                    ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
                }
            }

            Log(("ResizeDisplayDevice: %dx%dx%d at %d,%d\n",
                    paDeviceModes[DevNum].dmPelsWidth,
                    paDeviceModes[DevNum].dmPelsHeight,
                    paDeviceModes[DevNum].dmBitsPerPel,
                    paDeviceModes[DevNum].dmPosition.x,
                    paDeviceModes[DevNum].dmPosition.y));

            paRects[DevNum].left   = paDeviceModes[DevNum].dmPosition.x;
            paRects[DevNum].top    = paDeviceModes[DevNum].dmPosition.y;
            paRects[DevNum].right  = paDeviceModes[DevNum].dmPosition.x + paDeviceModes[DevNum].dmPelsWidth;
            paRects[DevNum].bottom = paDeviceModes[DevNum].dmPosition.y + paDeviceModes[DevNum].dmPelsHeight;
            DevNum++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
        DisplayDevice.cb = sizeof(DISPLAY_DEVICE);
        i++;
    }

    /* Width, height equal to 0 means that this value must be not changed.
     * Update input parameters if necessary.
     * Note: BitsPerPixel is taken into account later, when new rectangles
     *       are assigned to displays.
     */
    if (Width == 0)
    {
        Width = paRects[Id].right - paRects[Id].left;
    }

    if (Height == 0)
    {
        Height = paRects[Id].bottom - paRects[Id].top;
    }

    /* Check whether a mode reset or a change is requested. */
    if (   !fModeReset
        && paRects[Id].right - paRects[Id].left == Width
        && paRects[Id].bottom - paRects[Id].top == Height
        && paDeviceModes[Id].dmBitsPerPel == BitsPerPixel)
    {
        Log(("ResizeDisplayDevice: Already at desired resolution.\n"));
        return FALSE;
    }

    resizeRect(paRects, NumDevices, DevPrimaryNum, Id, Width, Height);
#ifdef Log
    for (i = 0; i < NumDevices; i++)
    {
        Log(("ResizeDisplayDevice: [%d]: %d,%d %dx%d\n",
                i, paRects[i].left, paRects[i].top,
                paRects[i].right - paRects[i].left,
                paRects[i].bottom - paRects[i].top));
    }
#endif /* Log */

    /* Without this, Windows will not ask the miniport for its
     * mode table but uses an internal cache instead.
     */
    DEVMODE tempDevMode;
    ZeroMemory (&tempDevMode, sizeof (tempDevMode));
    tempDevMode.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(NULL, 0xffffff, &tempDevMode);

    /* Assign the new rectangles to displays. */
    for (i = 0; i < NumDevices; i++)
    {
        paDeviceModes[i].dmPosition.x = paRects[i].left;
        paDeviceModes[i].dmPosition.y = paRects[i].top;
        paDeviceModes[i].dmPelsWidth  = paRects[i].right - paRects[i].left;
        paDeviceModes[i].dmPelsHeight = paRects[i].bottom - paRects[i].top;

        /* On Vista one must specify DM_BITSPERPEL.
         * Note that the current mode dmBitsPerPel is already in the DEVMODE structure.
         */
        paDeviceModes[i].dmFields = DM_POSITION | DM_PELSHEIGHT | DM_PELSWIDTH | DM_BITSPERPEL;

        if (   i == Id
            && BitsPerPixel != 0)
        {
            /* Change dmBitsPerPel if requested. */
            paDeviceModes[i].dmBitsPerPel = BitsPerPixel;
        }

        Log(("ResizeDisplayDevice: pfnChangeDisplaySettingsEx %x: %dx%dx%d at %d,%d\n",
              gCtx.pfnChangeDisplaySettingsEx,
              paDeviceModes[i].dmPelsWidth,
              paDeviceModes[i].dmPelsHeight,
              paDeviceModes[i].dmBitsPerPel,
              paDeviceModes[i].dmPosition.x,
              paDeviceModes[i].dmPosition.y));

        gCtx.pfnChangeDisplaySettingsEx((LPSTR)paDisplayDevices[i].DeviceName,
                                        &paDeviceModes[i], NULL, CDS_NORESET | CDS_UPDATEREGISTRY, NULL);
        Log(("ResizeDisplayDevice: ChangeDisplaySettingsEx position err %d\n", GetLastError ()));
    }

    /* A second call to ChangeDisplaySettings updates the monitor. */
    LONG status = ChangeDisplaySettings(NULL, 0);
    Log(("ResizeDisplayDevice: ChangeDisplaySettings update status %d\n", status));
    if (status == DISP_CHANGE_SUCCESSFUL || status == DISP_CHANGE_BADMODE)
    {
        /* Successfully set new video mode or our driver can not set the requested mode. Stop trying. */
        return FALSE;
    }

    /* Retry the request. */
    return TRUE;
}

/**
 * Thread function to wait for and process display change
 * requests
 */
unsigned __stdcall VBoxDisplayThread  (void *pInstance)
{
    VBOXDISPLAYCONTEXT *pCtx = (VBOXDISPLAYCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxDisplayThread : DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        Log(("VBoxDisplayThread : DeviceIOControl(CtlMask) failed, DisplayChangeThread exited\n"));
        return -1;
    }

    do
    {
        /* wait for a display change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 1000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
        if (DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            /*Log(("VBoxDisplayThread : DeviceIOControl succeded\n"));*/

            if (NULL == pCtx) {
                Log(("VBoxDisplayThread : Invalid context detected!\n"));
                break;
            }

            if (NULL == pCtx->pEnv) {
                Log(("VBoxDisplayThread : Invalid context environment detected!\n"));
                break;
            }

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            /*Log(("VBoxDisplayThread : checking event\n"));*/

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
            {
                Log(("VBoxDisplayThread : going to get display change information.\n"));

                /* We got at least one event. Read the requested resolution
                 * and try to set it until success. New events will not be seen
                 * but a new resolution will be read in this poll loop.
                 */
                for (;;)
                {
                    /* get the display change request */
                    VMMDevDisplayChangeRequest2 displayChangeRequest = {0};
                    displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest2);
                    displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                    displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest2;
                    displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                    BOOL fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevDisplayChangeRequest2)), &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2),
                                                                 &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2), &cbReturned, NULL);
                    if (!fDisplayChangeQueried)
                    {
                        /* Try the old version of the request for old VBox hosts. */
                        displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest);
                        displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                        displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest;
                        displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                        fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevDisplayChangeRequest)), &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest),
                                                                 &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest), &cbReturned, NULL);
                        displayChangeRequest.display = 0;
                    }

                    if (fDisplayChangeQueried)
                    {
                        Log(("VBoxDisplayThread : VMMDevReq_GetDisplayChangeRequest2: %dx%dx%d at %d\n", displayChangeRequest.xres, displayChangeRequest.yres, displayChangeRequest.bpp, displayChangeRequest.display));

                        /* Horizontal resolution must be a multiple of 8, round down. */
                        displayChangeRequest.xres &= 0xfff8;

                        /*
                         * Only try to change video mode if the active display driver is VBox additions.
                         */
                        if (isVBoxDisplayDriverActive (pCtx))
                        {
                            Log(("VBoxDisplayThread : Display driver is active!\n"));

                            if (pCtx->pfnChangeDisplaySettingsEx != 0)
                            {
                                Log(("VBoxDisplayThread : Detected W2K or later.\n"));

                                /* W2K or later. */
                                if (!ResizeDisplayDevice(displayChangeRequest.display,
                                                         displayChangeRequest.xres,
                                                         displayChangeRequest.yres,
                                                         displayChangeRequest.bpp))
                                {
                                    break;
                                }
                            }
                            else
                            {
                                Log(("VBoxDisplayThread : Detected NT.\n"));

                                /* Single monitor NT. */
                                DEVMODE devMode;
                                memset (&devMode, 0, sizeof (devMode));
                                devMode.dmSize = sizeof(DEVMODE);

                                /* get the current screen setup */
                                if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &devMode))
                                {
                                    Log(("VBoxDisplayThread : Current mode: %dx%dx%d at %d,%d\n", devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel, devMode.dmPosition.x, devMode.dmPosition.y));

                                    /* Check whether a mode reset or a change is requested. */
                                    if (displayChangeRequest.xres || displayChangeRequest.yres || displayChangeRequest.bpp)
                                    {
                                        /* A change is requested.
                                         * Set values which are not to be changed to the current values.
                                         */
                                        if (!displayChangeRequest.xres)
                                            displayChangeRequest.xres = devMode.dmPelsWidth;
                                        if (!displayChangeRequest.yres)
                                            displayChangeRequest.yres = devMode.dmPelsHeight;
                                        if (!displayChangeRequest.bpp)
                                            displayChangeRequest.bpp = devMode.dmBitsPerPel;
                                    }
                                    else
                                    {
                                        /* All zero values means a forced mode reset. Do nothing. */
                                        Log(("VBoxDisplayThread : Forced mode reset.\n"));
                                    }

                                    /* Verify that the mode is indeed changed. */
                                    if (   devMode.dmPelsWidth  == displayChangeRequest.xres
                                        && devMode.dmPelsHeight == displayChangeRequest.yres
                                        && devMode.dmBitsPerPel == displayChangeRequest.bpp)
                                    {
                                        Log(("VBoxDisplayThread : already at desired resolution.\n"));
                                        break;
                                    }

                                    // without this, Windows will not ask the miniport for its
                                    // mode table but uses an internal cache instead
                                    DEVMODE tempDevMode = {0};
                                    tempDevMode.dmSize = sizeof(DEVMODE);
                                    EnumDisplaySettings(NULL, 0xffffff, &tempDevMode);

                                    /* adjust the values that are supposed to change */
                                    if (displayChangeRequest.xres)
                                        devMode.dmPelsWidth  = displayChangeRequest.xres;
                                    if (displayChangeRequest.yres)
                                        devMode.dmPelsHeight = displayChangeRequest.yres;
                                    if (displayChangeRequest.bpp)
                                        devMode.dmBitsPerPel = displayChangeRequest.bpp;

                                    Log(("VBoxDisplayThread : setting the new mode %dx%dx%d\n", devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel));

                                    /* set the new mode */
                                    LONG status = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
                                    if (status != DISP_CHANGE_SUCCESSFUL)
                                    {
                                        Log(("VBoxDisplayThread : error from ChangeDisplaySettings: %d\n", status));

                                        if (status == DISP_CHANGE_BADMODE)
                                        {
                                            /* Our driver can not set the requested mode. Stop trying. */
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        /* Successfully set new video mode. */
                                        break;
                                    }
                                }
                                else
                                {
                                    Log(("VBoxDisplayThread : error from EnumDisplaySettings: %d\n", GetLastError ()));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            Log(("VBoxDisplayThread : vboxDisplayDriver is not active.\n"));
                        }

                        /* Retry the change a bit later. */
                        /* are we supposed to stop? */
                        if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
                        {
                            fTerminate = true;
                            break;
                        }
                    }
                    else
                    {
                        Log(("VBoxDisplayThread : error from DeviceIoControl VBOXGUEST_IOCTL_VMMREQUEST\n"));
                        /* sleep a bit to not eat too much CPU while retrying */
                        /* are we supposed to stop? */
                        if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 50) == WAIT_OBJECT_0)
                        {
                            fTerminate = true;
                            break;
                        }
                    }
                }
            }
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED)
                VBoxServiceReloadCursor();
        } else
        {
            Log(("VBoxDisplayThread : error 0 from DeviceIoControl VBOXGUEST_IOCTL_WAITEVENT\n"));
            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
    } while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxDisplayThread : DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        Log(("VBoxDisplayThread : DeviceIOControl(CtlMask) failed\n"));
    }

    Log(("VBoxDisplayThread : finished display change request thread\n"));
    return 0;
}
