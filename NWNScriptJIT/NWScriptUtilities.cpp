/*++

Copyright (c) Ken Johnson (Skywing). All rights reserved.

Module Name:

	NWScriptUtilities.cpp

Abstract:

	This module houses utility routine for the NWScript code generator and
	NWScript runtime environment.

--*/

#include "Precomp.h"
#include "NWScriptUtilities.h"


std::string
NWScriptUtilities::ConvertString(
	__in String ^ Str
	)
/*++

Routine Description:

	This routine converts a System::String to an std::string.

Arguments:

	Str - Supplies the string to convert.

Return Value:

	The converted std::string is returned.  On failure, a System::Exception, may
	be raised.

Environment:

	User mode, C++/CLI.

--*/
{
	#if 0
	using namespace Runtime::InteropServices;

	std::string  Sstr;
	const char * Chars = (const char *) (Marshal::StringToHGlobalAnsi( Str )).ToPointer( );
   
	try
	{
		Sstr = Chars;
	}
	catch (std::exception)
	{
		Marshal::FreeHGlobal( (IntPtr)( (void *) Chars) );

		throw gcnew Exception( "Failed to convert string." );
	}

	Marshal::FreeHGlobal( (IntPtr)( (void *) Chars) );

	return Sstr;
	#endif

	return msclr::interop::marshal_as< std::string >( Str );
}
