USE_OBJECT_ROOT=1

PROJECT_SOURCE_ROOT=$(_NTTREE)

EXTSDK_INC_PATH=$(PROJECT_SOURCE_ROOT)\External\WinSDK\inc;$(PROJECT_SOURCE_ROOT)\External\CRT\inc
EXTSDK_LIB_PATH=$(PROJECT_SOURCE_ROOT)\External\WinSDK\lib\*
NSS_INC_PATH=$(PROJECT_SOURCE_ROOT)\External\NWScript\inc
NSS_COMPILER=$(PROJECT_SOURCE_ROOT)\External\NWScript\exe\win32\x86\NWNScriptCompiler.exe

!if defined(386)
!if !defined(NWN2DEV_NO_SSE2)
USER_C_FLAGS=$(USER_C_FLAGS) /arch:SSE2
!endif
!elseif defined(AMD64)
!elseif defined(IA64)
!endif

LARGE_ADDRESS_AWARE=1

USER_C_DEFINES=$(USER_C_DEFINES) -D_NWN2DEV_

