/*++

Copyright (c) Ken Johnson (Skywing). All rights reserved.

Module Name:

	NWScriptUtilities.h

Abstract:

	This module defines utility routine for the NWScript code generator and
	NWScript runtime environment.

--*/

#ifndef _SOURCE_PROGRAMS_NWNSCRIPTJIT_NWSCRIPTUTILITIES_H
#define _SOURCE_PROGRAMS_NWNSCRIPTJIT_NWSCRIPTUTILITIES_H

#ifdef _MSC_VER
#pragma once
#endif

namespace NWScript
{

public ref class NWScriptUtilities
{

public:

	//
	// Convert a System::String into an std::string.
	//

	static
	std::string
	ConvertString(
		__in String ^ Str
		);



};

}

#endif
