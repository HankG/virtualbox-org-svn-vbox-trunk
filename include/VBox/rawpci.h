/** @file
 * PDM - Pluggable Device Manager, raw PCI Devices. (VMM)
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_rawpci_h
#define ___VBox_rawpci_h

#include <VBox/types.h>
#include <VBox/sup.h>

RT_C_DECLS_BEGIN

/**
 * Handle for the raw PCI device.
 */
typedef uint32_t PCIRAWDEVHANDLE;

/**
 * Physical memory action enumeration.
 */
typedef enum PCIRAWMEMINFOACTION
{
    /* Pages mapped. */
    PCIRAW_MEMINFO_MAP,
    /* Pages unmapped. */
    PCIRAW_MEMINFO_UNMAP,
    /** The usual 32-bit type blow up. */
    PCIRAW_MEMINFO_32BIT_HACK = 0x7fffffff
} PCIRAWMEMINFOACTION;

/**
 * Callback to notify raw PCI subsystem about mapping/unmapping of
 * host pages to the guest. Typical usecase is to register physical
 * RAM pages with IOMMU, so that it could allow DMA for PCI devices
 * directly from the guest RAM.
 * Region shall be one or more contigous (both host and guest) pages 
 * of physical memory.  
 * 
 * @param   pVM           VM pointer.
 * @param   HostStart     Physical address of region start on the host.  
 * @param   GuestStart    Physical address of region start on the guest.
 * @param   cMemSize      Region size in bytes.
 * @param   Action        Action performed.

 */
typedef DECLCALLBACK(void) FNRAWPCICONTIGPHYSMEMINFO(PVM pVM, RTHCPHYS HostStart, RTGCPHYS GuestStart, uint64_t cMemSize, PCIRAWMEMINFOACTION Action);
typedef FNRAWPCICONTIGPHYSMEMINFO *PFNRAWPCICONTIGPHYSMEMINFO;

/** Data being part of the VM structure. */
typedef struct RAWPCIVM
{
    /* Shall only be interpreted by the host PCI driver. */
    RTR0PTR                     pDriverData;
    /* Callback called when mapping of host pages to the guest changes. */
    PFNRAWPCICONTIGPHYSMEMINFO  pfnContigMemInfo;
} RAWPCIVM;
typedef RAWPCIVM *PRAWPCIVM;

/** Parameters buffer for PCIRAWR0_DO_OPEN_DEVICE call */
typedef struct
{
    /* in */
    uint32_t PciAddress;
    uint32_t fFlags;
    /* out */
    PCIRAWDEVHANDLE Device;
} PCIRAWREQOPENDEVICE;

/** Parameters buffer for PCIRAWR0_DO_CLOSE_DEVICE call */
typedef struct
{
    /* in */
    uint32_t fFlags;
} PCIRAWREQCLOSEDEVICE;

/** Parameters buffer for PCIRAWR0_DO_GET_REGION_INFO call */
typedef struct
{
    /* in */
    int32_t  iRegion;
    /* out */
    RTGCPHYS RegionStart;
    uint64_t u64RegionSize;
    bool     fPresent;
    uint32_t fFlags;
} PCIRAWREQGETREGIONINFO;

/** Parameters buffer for PCIRAWR0_DO_MAP_REGION call. */
typedef struct
{
    /* in */
    RTGCPHYS             StartAddress;
    uint64_t             iRegionSize;
    int32_t              iRegion;
    uint32_t             fFlags;
    /* out */
    RTR3PTR              pvAddressR3;
    RTR0PTR              pvAddressR0;
} PCIRAWREQMAPREGION;

/** Parameters buffer for PCIRAWR0_DO_UNMAP_REGION call. */
typedef struct
{
    /* in */
    RTGCPHYS             StartAddress;
    uint64_t             iRegionSize;
    RTR3PTR              pvAddressR3;
    RTR0PTR              pvAddressR0;
    int32_t              iRegion;
} PCIRAWREQUNMAPREGION;

/** Parameters buffer for PCIRAWR0_DO_PIO_WRITE call. */
typedef struct
{
    /* in */
    uint16_t             iPort;
    uint16_t             cb;
    uint32_t             iValue;
} PCIRAWREQPIOWRITE;

/** Parameters buffer for PCIRAWR0_DO_PIO_READ call. */
typedef struct
{
    /* in */
    uint16_t             iPort;
    uint16_t             cb;
    /* out */
    uint32_t             iValue;
} PCIRAWREQPIOREAD;

/** Memory operand. */
typedef struct
{
    union
    {
        uint8_t          u8;
        uint16_t         u16;
        uint32_t         u32;
        uint64_t         u64;
    } u;
    uint8_t cb;
} PCIRAWMEMLOC;

/** Parameters buffer for PCIRAWR0_DO_MMIO_WRITE call. */
typedef struct
{
    /* in */
    RTR0PTR              Address;
    PCIRAWMEMLOC         Value;
} PCIRAWREQMMIOWRITE;

/** Parameters buffer for PCIRAWR0_DO_MMIO_READ call. */
typedef struct
{
    /* in */
    RTR0PTR              Address;
    /* inout (Value.cb is in) */
    PCIRAWMEMLOC         Value;
} PCIRAWREQMMIOREAD;

/* Parameters buffer for PCIRAWR0_DO_PCICFG_WRITE call. */
typedef struct
{
    /* in */
    uint32_t             iOffset;
    PCIRAWMEMLOC         Value;
} PCIRAWREQPCICFGWRITE;

/** Parameters buffer for PCIRAWR0_DO_PCICFG_READ call. */
typedef struct
{
    /* in */
    uint32_t             iOffset;
    /* inout (Value.cb is in) */
    PCIRAWMEMLOC         Value;
} PCIRAWREQPCICFGREAD;

/** Parameters buffer for PCIRAWR0_DO_REGISTER_R0_IRQ_HANDLER call. */
typedef struct
{
    /* in */
    int32_t              iGuestIrq;
    RTR0PTR              pfnHandler;
    RTR0PTR              pfnHandlerContext;
    /* out */
    int32_t              iHostIrq;
} PCIRAWREQREGISTERR0IRQHANDLER;

/** Parameters buffer for PCIRAWR0_DO_UNREGISTER_R0_IRQ_HANDLER call. */
typedef struct
{
    /* in */
    int32_t              iHostIrq;
} PCIRAWREQUNREGISTERR0IRQHANDLER;

/** Parameters buffer for PCIRAWR0_DO_POWER_STATE_CHANGE call. */
typedef struct PCIRAWREQPOWERSTATECHANGE
{
    /* in */
    uint32_t             iState;
} PCIRAWREQPOWERSTATECHANGE;

/**
 * Request buffer use for communication with the driver.
 */
typedef struct PCIRAWSENDREQ
{
    /** The request header. */
    SUPVMMR0REQHDR  Hdr;
    /** Alternative to passing the taking the session from the VM handle.
     *  Either use this member or use the VM handle, don't do both.
     */
    PSUPDRVSESSION  pSession;
    /** Request type. */
    int32_t         iRequest;
    /** Host device request targetted to. */
    PCIRAWDEVHANDLE TargetDevice;
    /** Call parameters. */
    union
    {
        PCIRAWREQOPENDEVICE    aOpenDevice;
        PCIRAWREQCLOSEDEVICE   aCloseDevice;
        PCIRAWREQGETREGIONINFO aGetRegionInfo;
        PCIRAWREQMAPREGION     aMapRegion;
        PCIRAWREQUNMAPREGION   aUnmapRegion;
        PCIRAWREQPIOWRITE      aPioWrite;
        PCIRAWREQPIOREAD       aPioRead;
        PCIRAWREQMMIOWRITE     aMmioWrite;
        PCIRAWREQMMIOREAD      aMmioRead;
        PCIRAWREQPCICFGWRITE   aPciCfgWrite;
        PCIRAWREQPCICFGREAD    aPciCfgRead;
        PCIRAWREQREGISTERR0IRQHANDLER   aRegisterR0IrqHandler;
        PCIRAWREQUNREGISTERR0IRQHANDLER aUnregisterR0IrqHandler;
        PCIRAWREQPOWERSTATECHANGE aPowerStateChange;
    } u;
} PCIRAWSENDREQ;
typedef PCIRAWSENDREQ *PPCIRAWSENDREQ;

/**
 * Operations performed by the driver.
 */
typedef enum PCIRAWR0OPERATION
{
    /* Open device. */
    PCIRAWR0_DO_OPEN_DEVICE,
    /* Close device. */
    PCIRAWR0_DO_CLOSE_DEVICE,
    /* Get PCI region info. */
    PCIRAWR0_DO_GET_REGION_INFO,
    /* Map PCI region into VM address space. */
    PCIRAWR0_DO_MAP_REGION,
    /* Unmap PCI region from VM address space. */
    PCIRAWR0_DO_UNMAP_REGION,
    /* Perform PIO write. */
    PCIRAWR0_DO_PIO_WRITE,
    /* Perform PIO read. */
    PCIRAWR0_DO_PIO_READ,
    /* Perform MMIO write. */
    PCIRAWR0_DO_MMIO_WRITE,
    /* Perform MMIO read. */
    PCIRAWR0_DO_MMIO_READ,
    /* Perform PCI config write. */
    PCIRAWR0_DO_PCICFG_WRITE,
    /* Perform PCI config read. */
    PCIRAWR0_DO_PCICFG_READ,
    /* Register device IRQ R0 handler. */
    PCIRAWR0_DO_REGISTER_R0_IRQ_HANDLER,
    /* Unregister device IRQ R0 handler. */
    PCIRAWR0_DO_UNREGISTER_R0_IRQ_HANDLER,
    /* Notify driver about guest power state change. */
    PCIRAWR0_DO_POWER_STATE_CHANGE,
    /** The usual 32-bit type blow up. */
    PCIRAWR0_DO_32BIT_HACK = 0x7fffffff
} PCIRAWR0OPERATION;

/**
 * Power state enumeration.
 */
typedef enum PCIRAWPOWERSTATE
{
    /* Power on. */
    PCIRAW_POWER_ON,
    /* Power off. */
    PCIRAW_POWER_OFF,
    /* Suspend. */
    PCIRAW_POWER_SUSPEND,
    /* Resume. */
    PCIRAW_POWER_RESUME,  
    /** The usual 32-bit type blow up. */
    PCIRAW_POWER_32BIT_HACK = 0x7fffffff
} PCIRAWPOWERSTATE;


/** Forward declarations. */
typedef struct RAWPCIFACTORY *PRAWPCIFACTORY;
typedef struct RAWPCIDEVPORT *PRAWPCIDEVPORT;

/**
 * Interrupt service routine callback.
 *
 * @param   pvContext       Opaque user data which to the handler.
 * @param   iIrq            Interrupt number.
 */
typedef DECLCALLBACK(void) FNRAWPCIISR(void *pvContext, int32_t iIrq);
typedef FNRAWPCIISR *PFNRAWPCIISR;

/**
 * This is the port on the device interface, i.e. the driver side which the
 * host device is connected to.
 *
 * This is only used for the in-kernel PCI device connections.
 */
typedef struct RAWPCIDEVPORT
{
    /** Structure version number. (RAWPCIDEVPORT_VERSION) */
    uint32_t u32Version;

    /**
     * Init device.
     *
     * @param   pPort     Pointer to this structure.
     * @param   fFlags    Initialization flags.
     */
    DECLR0CALLBACKMEMBER(int,  pfnInit,(PRAWPCIDEVPORT pPort,
                                        uint32_t       fFlags));


    /**
     * Deinit device.
     *
     * @param   pPort     Pointer to this structure.
     * @param   fFlags    Initialization flags.
     */
    DECLR0CALLBACKMEMBER(int,  pfnDeinit,(PRAWPCIDEVPORT pPort,
                                          uint32_t       fFlags));


    /**
     * Destroy device.
     *
     * @param   pPort     Pointer to this structure.
     */
    DECLR0CALLBACKMEMBER(int,  pfnDestroy,(PRAWPCIDEVPORT pPort));

    /**
     * Get PCI region info.
     *
     * @param   pPort     Pointer to this structure.
     */
    DECLR0CALLBACKMEMBER(int,  pfnGetRegionInfo,(PRAWPCIDEVPORT pPort,
                                                 int32_t        iRegion,
                                                 RTHCPHYS       *pRegionStart,
                                                 uint64_t       *pu64RegionSize,
                                                 bool           *pfPresent,
                                                 uint32_t       *pfFlags));


    /**
     * Map PCI region.
     *
     * @param   pPort     Pointer to this structure.
     */
    DECLR0CALLBACKMEMBER(int,  pfnMapRegion,(PRAWPCIDEVPORT pPort,
                                             int32_t        iRegion,
                                             RTHCPHYS       RegionStart,
                                             uint64_t       u64RegionSize,
                                             int32_t        fFlags,
                                             RTR0PTR        *pRegionBaseR0));

    /**
     * Unmap PCI region.
     *
     * @param   pPort     Pointer to this structure.
     */
    DECLR0CALLBACKMEMBER(int,  pfnUnmapRegion,(PRAWPCIDEVPORT pPort,
                                               int32_t        iRegion,
                                               RTHCPHYS       RegionStart,
                                               uint64_t       u64RegionSize,
                                               RTR0PTR        RegionBase));

    /**
     * Read device PCI register.
     *
     * @param   pPort     Pointer to this structure.
     * @param   fFlags    Initialization flags.
     */
    DECLR0CALLBACKMEMBER(int,  pfnPciCfgRead,(PRAWPCIDEVPORT pPort,
                                              uint32_t          Register,
                                              PCIRAWMEMLOC      *pValue));


    /**
     * Write device PCI register.
     *
     * @param   pPort     Pointer to this structure.
     * @param   fFlags    Initialization flags.
     */
    DECLR0CALLBACKMEMBER(int,  pfnPciCfgWrite,(PRAWPCIDEVPORT pPort,
                                               uint32_t          Register,
                                               PCIRAWMEMLOC      *pValue));

    /**
     * Request to register interrupt handler.
     *
     * @param   pPort       Pointer to this structure.
     * @param   pfnHandler  Pointer to the handler.
     * @param   pIrqContext Context passed to the handler.
     * @param   piHostIrq   Which host IRQ is used.
     */
    DECLR0CALLBACKMEMBER(int,  pfnRegisterIrqHandler,(PRAWPCIDEVPORT pPort,
                                                      PFNRAWPCIISR pfnHandler,
                                                      void* pIrqContext,
                                                      int32_t *piHostIrq));

    /**
     * Request to unregister interrupt handler.
     *
     * @param   pPort       Pointer to this structure.
     * @param   iHostIrq    Which host IRQ was used (retured by earlier pfnRegisterIrqHandler).
     */
    DECLR0CALLBACKMEMBER(int,  pfnUnregisterIrqHandler,(PRAWPCIDEVPORT pPort,
                                                        int32_t iHostIrq));

    /**
     * Power state change notification.
     *
     * @param   pPort       Pointer to this structure.
     * @param   aState      New power state.
     */
    DECLR0CALLBACKMEMBER(int,  pfnPowerStateChange,(PRAWPCIDEVPORT pPort,
                                                    PCIRAWPOWERSTATE  aState));

    /** Structure version number. (RAWPCIDEVPORT_VERSION) */
    uint32_t u32VersionEnd;
} RAWPCIDEVPORT;
/** Version number for the RAWPCIDEVPORT::u32Version and RAWPCIIFPORT::u32VersionEnd fields. */
#define RAWPCIDEVPORT_VERSION   UINT32_C(0xAFBDCC01)

/**
 * The component factory interface for create a raw PCI interfaces.
 */
typedef struct RAWPCIFACTORY
{
    /**
     * Release this factory.
     *
     * SUPR0ComponentQueryFactory (SUPDRVFACTORY::pfnQueryFactoryInterface to be precise)
     * will retain a reference to the factory and the caller has to call this method to
     * release it once the pfnCreateAndConnect call(s) has been done.
     *
     * @param   pIfFactory          Pointer to this structure.
     */
    DECLR0CALLBACKMEMBER(void, pfnRelease,(PRAWPCIFACTORY pFactory));

    /**
     * Create an instance for the specfied host PCI card and connects it
     * to the driver.
     *
     *
     * @returns VBox status code.
     *
     * @param   pIfFactory          Pointer to this structure.
     * @param   u32HostAddress      Address of PCI device on the host.
     * @param   fFlags              Creation flags.
     * @param   pVmCtx              Context of VM where device is created.
     * @param   ppDevPort           Where to store the pointer to the device port
     *                              on success.
     *
     */
    DECLR0CALLBACKMEMBER(int, pfnCreateAndConnect,(PRAWPCIFACTORY       pFactory,
                                                   uint32_t             u32HostAddress,
                                                   uint32_t             fFlags,
                                                   PRAWPCIVM            pVmCtx,
                                                   PRAWPCIDEVPORT       *ppDevPort));


    /**
     * Initialize per-VM data related to PCI passthrough.
     *
     * @returns VBox status code.
     *
     * @param   pIfFactory          Pointer to this structure.
     * @param   pVM                 Pointer to VM structure to initialize.
     * @param   pPciData            Pointer to PCI data.
     */
    DECLR0CALLBACKMEMBER(int, pfnInitVm,(PRAWPCIFACTORY       pFactory,
                                         PVM                  pVM,
                                         PRAWPCIVM            pPciData));

    /**
     * Deinitialize per-VM data related to PCI passthrough.
     *
     * @returns VBox status code.
     *
     * @param   pIfFactory          Pointer to this structure.
     * @param   pVM                 Pointer to VM structure to deinitialize.
     * @param   pPciData            Pointer to PCI data.
     */
    DECLR0CALLBACKMEMBER(void, pfnDeinitVm,(PRAWPCIFACTORY       pFactory,
                                            PVM                  pVM,
                                            PRAWPCIVM            pPciData));
} RAWPCIFACTORY;

#define RAWPCIFACTORY_UUID_STR   "ea089839-4171-476f-adfb-9e7ab1cbd0fb"

/**
 * Flags passed to pfnPciDeviceConstructStart(), to notify driver
 * about options to be used to open device.
 */
typedef enum PCIRAWDRIVERFLAGS
{
    /** If runtime shall  try to detach host driver. */
    PCIRAWDRIVERRFLAG_DETACH_HOST_DRIVER   =  (1 << 0),
    /** The usual 32-bit type blow up. */
    PCIRAWDRIVERRFLAG_32BIT_HACK = 0x7fffffff
} PCIRAWDRIVERFLAGS;

/**
 * Flags used to describe PCI region, matches to PCIADDRESSSPACE
 * in pci.h.
 */
typedef enum PCIRAWADDRESSSPACE
{
    /** Memory. */
    PCIRAW_ADDRESS_SPACE_MEM = 0x00,
    /** I/O space. */
    PCIRAW_ADDRESS_SPACE_IO = 0x01,
    /** 32-bit BAR. */
    PCIRAW_ADDRESS_SPACE_BAR32 = 0x00,
    /** 64-bit BAR. */
    PCIRAW_ADDRESS_SPACE_BAR64 = 0x04,
    /** Prefetch memory. */
    PCIRAW_ADDRESS_SPACE_MEM_PREFETCH = 0x08,
    /** The usual 32-bit type blow up. */
    PCIRAW_ADDRESS_SPACE_32BIT_HACK = 0x7fffffff
} PCIRAWADDRESSSPACE;

RT_C_DECLS_END

#endif
