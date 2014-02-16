/*++

Copyright (c) Ken Johnson (Skywing). All rights reserved.

Module Name:

	UpdateModTemplates.cpp

Abstract:

	This module houses a sample program that updates placed object instances in
	a module with the most recent data available from their corresponding object
	templates.

	N.B.  Only directory mode modules are supported.

--*/

#include "Precomp.h"
#include "../NWN2DataLib/TextOut.h"
#include "../NWN2DataLib/ResourceManager.h"
#include "../NWN2DataLib/GffFileWriter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
typedef std::map<unsigned int, std::string> StringTable;
typedef std::hash_map<std::string, unsigned int> StringTableRev;

class StringTableLoader
{
private:

    StringTable _origin;
    StringTableRev _origin_rev;
    unsigned int _max_id;

    void PushString( std::stringstream &ss, const char splitter, unsigned int index ) 
    {
        std::string result = ss.str();
        int splitter_pos = result.find_last_of(splitter);
        PushString(index, splitter_pos < 0 ? result : result.substr(0, splitter_pos));

        ss.str(std::string());
    }

    unsigned int PushString(unsigned int index, const std::string& content)
    {
        StringTable::iterator ot = _origin.find(index);
        if(ot != _origin.end())
            throw std::runtime_error( "String ID conflict." );

        StringTableRev::iterator otr = _origin_rev.find(content);
        if(otr != _origin_rev.end())
            return otr->second; // duplicated string

        // got unique string
        _origin[index] = content;
        _origin_rev[content] = index;

        if(_max_id < index)
            _max_id = index;

        return index;
    }

public:

    static const unsigned int CUSTOM_BASE = 0x1000000;
    static const unsigned int INDEX_INVALID = 0xFFFFFFFF;

    StringTableLoader()
        :_max_id(0)
    {}

    unsigned int GetMaxID() const { return _max_id; }

    unsigned int PushString(const std::string& content)
    {
        return PushString(GetMaxID()+1, content);
    }

    void Save(const char* filename)
    {
        const std::string header("String #");
        const std::string header_next(" is ");
        const char splitter = '~';

        if(_origin.empty())
            return;

        std::ofstream  fin(filename);

        fin << "// Total:" << _origin.size() << std::endl;
        StringTable::iterator itr = _origin.begin();
        for(;itr!=_origin.end();itr++)
        {
            fin << header << itr->first << header_next << splitter
                << itr->second << splitter << std::endl;
        }
    }

    unsigned int Load(const char* filename)
    {
        _origin.clear();
        _origin_rev.clear();
        _max_id = 0;

        const std::string header("String #");
        const std::string header_next(" is ");
        const char splitter = '~';

        std::ifstream  fin(filename);
        std::string    line;

        unsigned int index = INDEX_INVALID;
        std::string content;
        std::stringstream ss(content);
        while(std::getline(fin, line))
        {
            //not including the \n 
            if( line.length() >= header.length() &&
                line.substr(0, header.length()) == header)
            {
                if(index != INDEX_INVALID)
                {
                    PushString(ss, splitter, index);
                }

                int pos = line.find_first_of(splitter);
                std::string index_str = line.substr(header.length(), pos - header_next.length() - header.length());
                index = atoi(index_str.c_str());

                ss << line.substr(pos + 1);
            }
            else if(index != INDEX_INVALID)
            {
                ss << std::endl;
                ss << line;
            }
        }

        if(index != INDEX_INVALID)
        {
            PushString(ss, splitter, index);
        }

        return _max_id;
    }
};

//
// Define the debug text output interface, used to write debug or log messages
// to the user.
//

class PrintfTextOut : public IDebugTextOut
{

public:

	inline
	PrintfTextOut(
		)
	{
		AllocConsole( );
	}

	inline
	~PrintfTextOut(
		)
	{
		FreeConsole( );
	}

	enum { STD_COLOR = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE };

	inline
	virtual
	void
	WriteText(
		__in __format_string const char* fmt,
		...
		)
	{
		va_list ap;

		va_start( ap, fmt );
		WriteTextV( STD_COLOR, fmt, ap );
		va_end( ap );
	}

	inline
	virtual
	void
	WriteText(
		__in WORD Attributes,
		__in __format_string const char* fmt,
		...
		)
	{
		va_list ap;

		va_start( ap, fmt );
		WriteTextV( Attributes, fmt, ap );
		va_end( ap );

		UNREFERENCED_PARAMETER( Attributes );
	}

	inline
	virtual
	void
	WriteTextV(
		__in __format_string const char* fmt,
		__in va_list ap
		)
	{
		WriteTextV( STD_COLOR, fmt, ap );
	}

	inline
	virtual
	void
	WriteTextV(
		__in WORD Attributes,
		__in const char *fmt,
		__in va_list argptr
		)
	/*++

	Routine Description:

		This routine displays text to the log file and the debug console.

		The console output may have color attributes supplied, as per the standard
		SetConsoleTextAttribute API.

	Arguments:

		Attributes - Supplies color attributes for the text as per the standard
					 SetConsoleTextAttribute API (e.g. FOREGROUND_RED).

		fmt - Supplies the printf-style format string to use to display text.

		argptr - Supplies format inserts.

	Return Value:

		None.

	Environment:

		User mode.

	--*/
	{
		vprintf(fmt, argptr);
		//HANDLE console = GetStdHandle( STD_OUTPUT_HANDLE );
		//char buf[8193];
		//StringCbVPrintfA(buf, sizeof( buf ), fmt, argptr);
		//DWORD n = (DWORD)strlen(buf);
		//SetConsoleTextAttribute( console, Attributes );
		//WriteConsoleA(console, buf, n, &n, 0);
	}

};

struct ObjectTypeDescriptor
{
	NWN::OBJECT_TYPE   TypeCode;
	const char       * TypeName;
	const char       * InstanceListName;
	NWN::ResType       TemplateResType;
};

static const ObjectTypeDescriptor ValidObjectTypes[ ] =
{
	{ NWN::OBJECT_TYPE_TREE              , "tree"             , "TreeList"         , NWN::ResUTR     },
	{ NWN::OBJECT_TYPE_WAYPOINT          , "waypoint"         , "WaypointList"     , NWN::ResUTW     },
	{ NWN::OBJECT_TYPE_PLACED_EFFECT     , "placedeffect"     , "PlacedFXList"     , NWN::ResUPE     },
	{ NWN::OBJECT_TYPE_PLACEABLE         , "placeable"        , "Placeable List"   , NWN::ResUTP     },
	{ NWN::OBJECT_TYPE_DOOR              , "door"             , "Door List"        , NWN::ResUTD     },
	{ NWN::OBJECT_TYPE_ITEM              , "item"             , "List"             , NWN::ResUTI     },
	{ NWN::OBJECT_TYPE_ENVIRONMENT_OBJECT, "environmentobject", "EnvironmentList"  , NWN::ResUTP     },
	{ NWN::OBJECT_TYPE_LIGHT             , "light"            , "LightList"        , NWN::ResINVALID },
	{ NWN::OBJECT_TYPE_CREATURE          , "creature"         , "Creature List"    , NWN::ResUTC     },
	{ NWN::OBJECT_TYPE_TRIGGER           , "trigger"          , "TriggerList"      , NWN::ResUTT     },
	{ NWN::OBJECT_TYPE_SOUND             , "sound"            , "SoundList"        , NWN::ResUTS     },
	{ NWN::OBJECT_TYPE_STATIC_CAMERA     , "staticcamera"     , "StaticCameraList" , NWN::ResUSC     },
	{ NWN::OBJECT_TYPE_STORE             , "store"            , "StoreList"        , NWN::ResUTM     }
};

static const size_t NumValidObjectTypes = sizeof( ValidObjectTypes ) / sizeof( ValidObjectTypes[ 0 ] );

static bool DryRun = true;
static bool ExportCustom = false;
static StringTableLoader CustomStringTable;

typedef std::vector< std::string > StringVec;

void ExportStringField( __in const GffFileReader::GffStruct &StructIn, 
                        __in const char * FieldName, 
                        __in IDebugTextOut * TextOut, 
                        __inout GffFileWriter::GffStruct &StructOut )
{
    std::string ListFieldData;
    StructIn.GetCExoLocString( FieldName, ListFieldData);
    if(!ListFieldData.empty())
    {
        GffFileReader::GFF_CEXOLOCSTRING_ENTRY LocString;
        StructIn.GetCExoLocStringRef( FieldName, LocString);

        TextOut->WriteText("[OUTPUT_FIELD]\n");
        if(LocString.StringCount > 0)
        {
            if(LocString.StringRef != StringTableLoader::INDEX_INVALID)
            {
                TextOut->WriteText("[EMBEDDED_STRING_WITH_REF]%d\n", LocString.StringRef);
            }
            unsigned int index = CustomStringTable.PushString(ListFieldData.c_str( ));
            TextOut->WriteText("[CUSTOM_INDEX]%d\n", index);

            if(ExportCustom)
            {
                StructOut.DeleteField( FieldName );
                StructOut.SetCExoLocStringRef( FieldName, StringTableLoader::CUSTOM_BASE + index);
            }
        }
        TextOut->WriteText("[GFF_CEXOLOCSTRING],%s,~%s~\n", FieldName, ListFieldData.c_str( ));
        TextOut->WriteText("[/OUTPUT_FIELD]\n");
    }
}

void UpdateListField( __in const GffFileReader::GffStruct * Struct, 
                      __in GffFileWriter::GffStruct * StructOut, 
                      __in const char * FieldName, 
                      __in IDebugTextOut * TextOut, 
                      __in int Deep ) 
{
    GffFileReader::GffStruct ListStruct;
    GffFileWriter::GffStruct ListStructOut;

    for (size_t ListIndex = 0; ListIndex <= ULONG_MAX; ListIndex+= 1)
    {
        if (!Struct->GetListElement(FieldName, ListIndex, ListStruct))
        {
            break;
        }

        if (!StructOut->GetListElement(FieldName, ListIndex, ListStructOut))
        {
            throw std::runtime_error( "Internal error: GFF List reader/writer out of sync." );
        }

        for (GffFileReader::FIELD_INDEX ListFieldIdx = 0; ListFieldIdx < ListStruct.GetFieldCount( ); ListFieldIdx += 1)
        {
            GffFileReader::GFF_FIELD_TYPE ListFieldType;
            std::string                   ListFieldName;

            if (!ListStruct.GetFieldName( ListFieldIdx, ListFieldName ))
                throw std::runtime_error( "GetFieldName failed on Struct." );

            if (!ListStruct.GetFieldType( ListFieldName.c_str( ), ListFieldType ))
                continue;

            for(int i=0;i<Deep;i++) TextOut->WriteText(" ");
            TextOut->WriteText("[LIST_FIELD],%d,%s\n", ListFieldType, ListFieldName.c_str( ));

            if(GffFileReader::GFF_LIST == ListFieldType && Deep < 32)
            {
                UpdateListField(&ListStruct, &ListStructOut, ListFieldName.c_str( ), TextOut, Deep + 1);
            }
            else if(GffFileReader::GFF_CEXOLOCSTRING == ListFieldType)
            {
                ExportStringField(ListStruct, ListFieldName.c_str(), TextOut, ListStructOut);
            }
        }
    }
}

void
UpdateObjectInstanceFromTemplate(
	__in const GffFileReader::GffStruct * TemplateStruct,
	__in const GffFileReader::GffStruct * ObjStructIn,
	__inout GffFileWriter::GffStruct * ObjStructOut,
	__in const StringVec & ExcludeFields,
    __in const StringVec & IncludeFields,
	__in IDebugTextOut * TextOut
	)
/*++

Routine Description:

	This routine recopies all data from an object template over to an object
	instance.

Arguments:

	TemplateStruct - Supplies the GFF struct for the objects template.

	ObjStructIn - Supplies the original GFF struct for the object instance.

	ObjStructOut - Supplies the output GFF struct for the modified object
	               instance.  The output GFF struct is initially initialized to
	               have the same contents as the input GFF struct.

	ExcludeFields - Supplies a list of fields that are to be excluded from
	                copying even if they are present in the template.

	TextOut - Supplies the text output interface.

Return Value:

	None.  An std::exception is raised on failure.

Environment:

	User mode.

--*/
{
	UNREFERENCED_PARAMETER( ObjStructIn );
	UNREFERENCED_PARAMETER( TextOut );

	//
	// Loop through all fields in this level of the template structure, and copy
	// them over to the modified structure.  We do not use the bulk copy API as
	// we don't want to delete existing contents should the object instance have
	// more entries in a particular structure than the template did.
	//
	// N.B.  Any sub-structs or lists defined in the template cause the original
	//       fields to be completely replaced with the data from the template !
	//

	for (GffFileReader::FIELD_INDEX FieldIdx = 0;
	     FieldIdx < TemplateStruct->GetFieldCount( );
	     FieldIdx += 1)
	{
		GffFileReader::GFF_FIELD_TYPE FieldType;
		std::string                   FieldName;

		if (!TemplateStruct->GetFieldName( FieldIdx, FieldName ))
			throw std::runtime_error( "GetFieldName failed on TemplateStruct." );

		//
		// If we are to exclude this field from updating, then do so now.
		//

		if (!ExcludeFields.empty() &&
            std::find(
			ExcludeFields.begin( ),
			ExcludeFields.end( ),
			FieldName ) != ExcludeFields.end( ))
		{
			continue;
		}

        if (!IncludeFields.empty() &&
            std::find(
            IncludeFields.begin( ),
            IncludeFields.end( ),
            FieldName ) == IncludeFields.end( ))
        {
            continue;
        }

		//
		// Don't copy fields which are present in the template but -not- the
		// instance, as these are typically special toolset-only fields, like the
		// toolset comments.
		//

		if (!ObjStructIn->GetFieldType( FieldName.c_str( ), FieldType ))
			continue;

        TextOut->WriteText("[FIELD],%d,%s\n", FieldType, FieldName.c_str( ));

        if(GffFileReader::GFF_LIST == FieldType)
        {
            UpdateListField(ObjStructIn, ObjStructOut, FieldName.c_str( ), TextOut, 0);
        }
        else if(GffFileReader::GFF_CEXOLOCSTRING == FieldType)
        {
            ExportStringField(*ObjStructIn, FieldName.c_str(), TextOut, *ObjStructOut);
        }

		//
		// Delete the original field contents of this field and replace them with
		// those from the template.
		//

        if(!DryRun && !ExportCustom)
        {
			ObjStructOut->DeleteField( FieldName.c_str( ) );
			ObjStructOut->CopyField( TemplateStruct, FieldIdx );
        }
	}
}

void
ProcessArea(
	__in const NWN::ResRef32 & AreaResRef,
	__in ResourceManager & ResMan,
	__in IDebugTextOut * TextOut,
	__in unsigned long ObjectTypeMask,
	__in const StringVec & TemplateNames,
	__in const StringVec & ExcludeFields,
    __in const StringVec & IncludeFields
	)
/*++

Routine Description:

	This routine updates placed instances within a given area with data from
	their templates.

Arguments:

	AreaResRef - Supplies the resource name of the area to display.

	ResMan - Supplies a reference to the resource manager instance to use in
	         order to load any associated resource data.

	TextOut - Supplies the text output interface.

	ObjectTypeMask - Supplies the mask of object types to update templates for.

	TemplateNames - Supplies the RESREF names of templates that are to be
	                updated.

	ExcludeFields - Supplies a list of fields that are to be excluded from
	                copying even if they are present in the template.

Return Value:

	None.  An std::exception is raised on failure.

Environment:

	User mode.

--*/
{
	//
	// Areas are comprised of two files, an <area>.are with area parameters,
	// and an <area>.git with the object instance parameters about objects that
	// have been placed in the area via the toolset.
	//

	DemandResource32                 AreFile( ResMan, AreaResRef, NWN::ResARE );
	DemandResource32                 GitFile( ResMan, AreaResRef, NWN::ResGIT );
	GffFileReader                    Are( AreFile, ResMan );
	GffFileReader::Ptr               Git = new GffFileReader( GitFile, ResMan );
	GffFileWriter                    GitWriter;
	const GffFileReader::GffStruct * RootStruct;
	GffFileWriter::GffStruct         GitWriterRoot;
	std::string                      AreaName;
	std::string                      AreaTag;

	//
	// Start off by duplicating the current GIT contents over to the new output
	// GIT.
	//

	RootStruct = Are.GetRootStruct( );
	GitWriter.InitializeFromReader( Git.get( ) );

	//
	// Acquire parameters we need from area.git.
	//

	if (!RootStruct->GetCExoLocString( "Name", AreaName ))
		throw std::runtime_error( "Failed to read area Name" );
	if (!RootStruct->GetCExoString( "Tag", AreaTag ))
		throw std::runtime_error( "Failed to read area Tag" );
	
	TextOut->WriteText(
		"Updating instance information for area %s (tag %s)...\n", 
		AreaName.c_str( ),
		AreaTag.c_str( ));

	//
	// Now update each of the instance data items that we are interested in.
	//

	RootStruct    = Git->GetRootStruct( );
	GitWriterRoot = GitWriter.GetRootStruct( );

	for (size_t i = 0; i < NumValidObjectTypes; i += 1)
	{
		if (ObjectTypeMask > 0 && !(ObjectTypeMask & (1 << ValidObjectTypes[ i ].TypeCode )))
			continue;

		//
		// This is an object type we're interested in, scan for objects that have
		// a template we're to refresh and copy the data.
		//

		for (size_t j = 0; j <= ULONG_MAX; j += 1)
		{
			GffFileReader::GffStruct ObjStructIn;
			GffFileWriter::GffStruct ObjStructOut;
			NWN::ResRef32            TemplateResRef;
			std::string              TemplateString;
			bool                     MatchingTemplate;
			std::string              FileName;
			GffFileReader::Ptr       TemplateReader;

			//
			// Fetch the corresponding list element in both the input and output
			// GITs so that we can make modifications as necessary.
			//

			if (!RootStruct->GetListElement( ValidObjectTypes[ i ].InstanceListName, j, ObjStructIn ))
				break;

			if (!GitWriterRoot.GetListElement( ValidObjectTypes[ i ].InstanceListName, j, ObjStructOut ))
				throw std::runtime_error( "Internal error: GFF reader/writer out of sync." );

			//
			// If the object instance had no associated template, there's nothing
			// for us to update, so skip it.
			//

			if (!ObjStructIn.GetResRef( "TemplateResRef", TemplateResRef ))
				continue;

			TemplateString   = ResMan.StrFromResRef( TemplateResRef );
			MatchingTemplate = TemplateNames.empty(); // if empty then ignore filter

			for (StringVec::const_iterator it = TemplateNames.begin( );
			     it != TemplateNames.end( );
			     ++it)
			{
				if (!_stricmp( it->c_str( ), TemplateString.c_str( ) ))
				{
					MatchingTemplate = true;
					break;
				}
			}

			if (!MatchingTemplate)
				continue;

			//
			// This instance appears to be one that we should update, try and
			// process it.
			//

			TextOut->WriteText(
				"Refreshing template data for object #%lu of type %s (template %s.%s)...\n",
				(unsigned long) j,
				ValidObjectTypes[ i ].TypeName,
				TemplateString.c_str( ),
				ResMan.ResTypeToExt( ValidObjectTypes[ i ].TemplateResType ));

			//
			// Note that we must be careful here, as not only may objects have bad
			// template RESREFs, they may also have RESREFs to files that are not
			// even legal GFF-based templates to begin with! (e.g. fireplace.upe).
			//

			try
			{
				FileName = ResMan.Demand(
					TemplateResRef,
					ValidObjectTypes[ i ].TemplateResType);
			}
			catch (std::exception &e)
			{
				TextOut->WriteText(
					"WARNING:  Exception '%s' locating template %s.%s, skipping object instance...\n",
					e.what( ),
					TemplateString.c_str( ),
					ResMan.ResTypeToExt( ValidObjectTypes[ i ].TemplateResType ));

				continue;
			}

			try
			{
				TemplateReader = new GffFileReader(
					FileName,
					ResMan);
			}
			catch (std::exception &e)
			{
				TextOut->WriteText(
					"WARNING:  Exception '%s' loading template %s.%s, skipping object instance...\n",
					e.what( ),
					TemplateString.c_str( ),
					ResMan.ResTypeToExt( ValidObjectTypes[ i ].TemplateResType ));
				ResMan.Release( FileName );

				continue;
			}

			//
			// Finally, update the instance data.
			//

			try
			{
				UpdateObjectInstanceFromTemplate(
					TemplateReader->GetRootStruct( ),
					&ObjStructIn,
					&ObjStructOut,
					ExcludeFields,
                    IncludeFields,
					TextOut);
			}
			catch (std::exception &e)
			{
				TextOut->WriteText(
					"WARNING:  Exception '%s' refreshing object instance from template %s.%s, skipping object instance...\n",
					e.what( ),
					TemplateString.c_str( ),
					ResMan.ResTypeToExt( ValidObjectTypes[ i ].TemplateResType ));
			}

			TemplateReader = NULL;
			ResMan.Release( FileName );
		}
	}

	//
	// Now replace the object instance GFF with our edited version.
	//

	Git = NULL;

	GitWriter.Commit(
		GitFile,
		GffFileWriter::GIT_FILE_TYPE,
		GffFileWriter::GFF_COMMIT_FLAG_SEQUENTIAL);
}

void
PrintErrorBadObjectType(
	)
/*++

Routine Description:

	This routine prints an error message if an invalid object type is
	specified.

Arguments:

	None.

Return Value:

	None.

Environment:

	User mode.

--*/
{
	printf( "Invalid object type specified.  Legal object types are as follows:\n" );

	for (size_t i = 0; i < NumValidObjectTypes; i += 1)
		printf( "   %s\n", ValidObjectTypes[ i ].TypeName );
}

void
PrintUsage(
	)
/*++

Routine Description:

	This routine prints usage information for the program to the console.

Arguments:

	None.

Return Value:

	None.

Environment:

	User mode.

--*/
{
	printf(
		"UpdateModTemplates\n"
		"\n"
		"This program copies data from the specified template(s) to placed object\n"
		"instances of the given object type(s) within a directory mode module.\n"
		"Only directory mode modules are supported!\n"
		"\n"
		"Optionally, a list of GFF fields may be excluded from updating via the\n"
		"usage of the -excludefield parameter (for example,\n"
		"-excludefield ModelScale).  Use a GFF editor to pick the fields to\n"
		"exclude (if desired).\n"
		"\n"
		"Usage: UpdateModTemplates -home <homedir> -installdir <installdir>\n"
		"                          -module <module resource name> [-nwn1]\n"
		"                          -template <first template name to update>\n"
		"                          [-template <additional template name N...>]\n"
		"                          -objecttype <first object type to match>\n"
		"                          [-objecttype <additional object type N...>]\n"
		"                          [-excludefield <exclude field 1...>]\n"
		);

	printf( "\n" );
	printf( "Legal object types are:\n" );

	for (size_t i = 0; i < NumValidObjectTypes; i += 1)
		printf( "   %s\n", ValidObjectTypes[ i ].TypeName );
}

void
LoadModule(
	__in ResourceManager & ResMan,
	__in const char * ModuleName,
	__in const char * NWN2Home,
	__in const char * InstallDir,
	__in bool Erf16
	)
/*++

Routine Description:

	This routine performs a full load of a module, including the TLK file and
	any dependent HAKs.

Arguments:

	ResMan - Supplies the ResourceManager instance that is to load the module.

	ModuleName - Supplies the resource name of the module to load.

	NWN2Home - Supplies the users NWN2 home directory (i.e. NWN2 Documents dir).

	InstallDir - Supplies the game installation directory.

	Erf16 - Supplies a Boolean value indicating true if 16-byte ERFs are to be
	        used (i.e. for NWN1-style modules), else false if 32-byte ERFs are
	        to be used (i.e. for NWN2-style modules).

Return Value:

	None.  On failure, an std::exception is raised.

Environment:

	User mode.

--*/
{
	std::vector< NWN::ResRef32 >        HAKList;
	std::string                         CustomTlk;
	ResourceManager::ModuleLoadParams   LoadParams;

	ZeroMemory( &LoadParams, sizeof( LoadParams ) );

	//
	// Load up the module.  First, we load just the core module resources, then
	// we determine the HAK list and load all of the HAKs up too.
	//
	// Turn off granny2 loading as it's unnecessary for this program, and prefer
	// to load directory modules (as changes to ERF modules aren't saved).
	//

	LoadParams.SearchOrder = ResourceManager::ModSearch_PrefDirectory;
	LoadParams.ResManFlags = ResourceManager::ResManFlagNoGranny2 |
	                         ResourceManager::ResManFlagLoadCoreModuleOnly;

	if (Erf16)
		LoadParams.ResManFlags |= ResourceManager::ResManFlagErf16;

	ResMan.LoadModuleResources(
		ModuleName,
		"",
		NWN2Home,
		InstallDir,
		HAKList,
		&LoadParams);

	{
		DemandResourceStr                ModuleIfoFile( ResMan, "module", NWN::ResIFO );
		GffFileReader                    ModuleIfo( ModuleIfoFile, ResMan );
		const GffFileReader::GffStruct * RootStruct = ModuleIfo.GetRootStruct( );
		GffFileReader::GffStruct         Struct;
		size_t                           Offset;

		RootStruct->GetCExoString( "Mod_CustomTlk", CustomTlk );

		//
		// Chop off the .tlk extension in the CustomTlk field if we had one.
		//

		if ((Offset = CustomTlk.rfind( '.' )) != std::string::npos)
			CustomTlk.erase( Offset );

		for (size_t i = 0; i <= UCHAR_MAX; i += 1)
		{
			GffFileReader::GffStruct Hak;
			NWN::ResRef32            HakRef;

			if (!RootStruct->GetListElement( "Mod_HakList", i, Hak ))
				break;

			if (!Hak.GetCExoStringAsResRef( "Mod_Hak", HakRef ))
				throw std::runtime_error( "Failed to read Mod_HakList.Mod_Hak" );

			HAKList.push_back( HakRef );
		}

		//
		// If there were no haks, then try the legacy field.
		//

		if (HAKList.empty( ))
		{
			NWN::ResRef32 HakRef;

			if ((RootStruct->GetCExoStringAsResRef( "Mod_Hak", HakRef )) &&
				 (HakRef.RefStr[ 0 ] != '\0'))
			{
				HAKList.push_back( HakRef );
			}
		}
	}

	//
	// Now perform a full load with the HAK list and CustomTlk available.
	//
	// N.B.  The DemandResourceStr above must go out of scope before we issue a
	//       new load, as it references a temporary file that will be cleaned up
	//       by the new load request.
	//

	ZeroMemory( &LoadParams, sizeof( LoadParams ) );

	//
	// Load up the module.  First, we load just the core module resources, then
	// we determine the HAK list and load all of the HAKs up too.
	//
	// Turn off granny2 loading as it's unnecessary for this program, and prefer
	// to load directory modules (as changes to ERF modules aren't saved).
	//

	LoadParams.SearchOrder = ResourceManager::ModSearch_PrefDirectory;
	LoadParams.ResManFlags = ResourceManager::ResManFlagNoGranny2;

	if (Erf16)
		LoadParams.ResManFlags |= ResourceManager::ResManFlagErf16;

	ResMan.LoadModuleResources(
		ModuleName,
		CustomTlk,
		NWN2Home,
		InstallDir,
		HAKList,
		&LoadParams
		);
}

int
__cdecl
main(
	__in int argc,
	__in_ecount( argc ) const char * * argv
	)
/*++

Routine Description:

	This routine is the entry point symbol for the module instance template
	updater program.

Arguments:

	argc - Supplies the count of command line arguments.

	argv - Supplies the command line argument vector.

Return Value:

	The routine returns the process exit code.

Environment:

	User mode.

--*/
{
	const char    * ModuleName;
	const char    * NWN2Home;
	const char    * InstallDir;
	StringVec       TemplateNames;
	StringVec       ExcludeFields;
    StringVec       IncludeFields;
	unsigned long   ObjectTypeMask;
	bool            Erf16;
    const char    * CustomInput = NULL;
    const char    * CustomOutput = NULL;

	ModuleName     = NULL;
	NWN2Home       = NULL;
	InstallDir     = NULL;
	ObjectTypeMask = 0;
	Erf16          = false;

	//
	// Parse out the command line arguments.
	//

	for (int i = 1; i < argc; i += 1)
	{
		if ((!_stricmp( argv[ i ], "-module" )) && (i + 1 < argc))
			ModuleName = argv[ ++i ];
		else if ((!_stricmp( argv[ i ], "-home" )) && (i + 1 < argc))
			NWN2Home = argv[ ++i ];
		else if ((!_stricmp( argv[ i ], "-installdir" )) && (i + 1 < argc))
			InstallDir = argv[ ++i ];
		else if ((!_stricmp( argv[ i ], "-template" )) && (i + 1 < argc))
			TemplateNames.push_back( argv[ ++i ] );
		else if ((!_stricmp( argv[ i ], "-excludefield" )) && (i + 1 < argc))
			ExcludeFields.push_back( argv[ ++i ] );
		else if ((!_stricmp( argv[ i ], "-includefield" )) && (i + 1 < argc))
			IncludeFields.push_back( argv[ ++i ] );
		else if ((!_stricmp( argv[ i ], "-nwn1")))
			Erf16 = true;
		else if ((!_stricmp( argv[ i ], "-save")))
			DryRun = false;
        else if ((!_stricmp( argv[ i ], "-export")))
            ExportCustom = true;
        else if ((!_stricmp( argv[ i ], "-custominput" )) && (i + 1 < argc))
            CustomInput = argv[ ++i ];
        else if ((!_stricmp( argv[ i ], "-customoutput" )) && (i + 1 < argc))
            CustomOutput = argv[ ++i ];
		else if ((!_stricmp( argv[ i ], "-objecttype" )) && (i + 1 < argc))
		{
			bool FoundIt;

			FoundIt  = false;
			i       += 1;

			for (size_t j = 0; j < NumValidObjectTypes; j += 1)
			{
				if (!_stricmp( argv[ i ], ValidObjectTypes[ j ].TypeName ))
				{
					ObjectTypeMask |= 1 << ValidObjectTypes[ j ].TypeCode;
					FoundIt         = true;
					break;
				}
			}

			if (!FoundIt)
			{
				PrintErrorBadObjectType( );
				return -1;
			}
		}
		else
		{
			PrintUsage( );
			printf( "\nUnrecognized command line argument.\n" );
			return -1;
		}
	}

	//
	// First, check that we've got the necessary arguments.
	//

	if (ModuleName == NULL)
	{
		PrintUsage( );
		printf( "\nYou must specify the module resource name of the module to load with -module <module resource name>.  This is the name of the module directory.  The module resource name must be enclosed in quotes if it contains spaces.\n" );
		return -1;
	}

	if (NWN2Home == NULL)
	{
		PrintUsage( );
		printf( "\nYou must specify the NWN2 home directory location with -home <homedir>.  The home directory is typically the path to your \"Documents\\Neverwinter Nights 2\" directory.  The directory name must be enclosed in quotes if it contains spaces.\n" );
		return -1;
	}

	if (InstallDir == NULL)
	{
		PrintUsage( );
		printf( "\nYou must specify the NWN2 game installation directory location with -installdir <installdir>.  The installation directory is typically the path to the Neverwinter Nights 2 directory under Program Files.  The directory name must be enclosed in quotes if it contains spaces.\n" );
		return -1;
	}

	//if (ObjectTypeMask == 0)
	//{
	//	PrintUsage( );
	//	printf( "\nYou must specify at least one object type to match (with -objecttype <typename>).\n" );
	//	return -1;
	//}

	//if (TemplateNames.empty( ))
	//{
	//	PrintUsage( );
	//	printf( "\nYou must specify at least one template RESREF (no extension) to match (with -template <resref>).\n" );
	//	return -1;
	//}
	
	//
	// Now spin up a resource manager instance.
	//

	PrintfTextOut   TextOut;
	ResourceManager ResMan( &TextOut );

    if(CustomInput)
    {
        TextOut.WriteText( "Load Custom StringTable...\n" );
        CustomStringTable.Load(CustomInput);
        TextOut.WriteText( "Custom StringTable end with %d.\n", CustomStringTable.GetMaxID() );
    }

	try
	{
		//
		// First, load up the module.
		//

		TextOut.WriteText( "Loading module...\n" );
		LoadModule( ResMan, ModuleName, NWN2Home, InstallDir, Erf16 );

		//
		// Acquire a file name for module.ifo and load it up using the GFF
		// reader library.
		//

		DemandResourceStr                ModuleIfoFile( ResMan, "module", NWN::ResIFO );
		GffFileReader                    ModuleIfo( ModuleIfoFile, ResMan );
		const GffFileReader::GffStruct * RootStruct = ModuleIfo.GetRootStruct( );
		std::string                      ModName;
		GffFileReader::GffStruct         Struct;

		if (RootStruct->GetCExoLocString( "Mod_Name", ModName ))
			TextOut.WriteText( "The module name is: %s.\n", ModName.c_str( ) );

		//
		// Now look at each area.
		//

		for (size_t i = 0; i <= ULONG_MAX; i += 1)
		{
			NWN::ResRef32 AreaResRef;

			if (!RootStruct->GetListElement( "Mod_Area_list", i, Struct ))
				break;

			if (!Struct.GetResRef( "Area_Name", AreaResRef ))
				throw std::runtime_error( "Mod_Area_list element is missing Area_Name." );

			//
			// Process templates in this area.
			//

			ProcessArea(
				AreaResRef,
				ResMan,
				&TextOut,
				ObjectTypeMask,
				TemplateNames,
				ExcludeFields,
                IncludeFields);
		}

		TextOut.WriteText( "Finished processing module.\n" );
	}
	catch (std::exception &e)
	{
		//
		// Simple print an error message and abort if we went wrong, such as if
		// we couldn't load the module.
		//

		TextOut.WriteText( "ERROR: Exception '%s'.\n", e.what( ) );
	}

    if(CustomOutput)
    {
        TextOut.WriteText( "Save Custom StringTable.\n" );
        CustomStringTable.Save(CustomOutput);
    }

	//
	// All done.
	//

	return 0;
}
