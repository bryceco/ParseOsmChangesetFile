//
//  parser.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/2/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#include <string>

#include "ChangesetParser.hpp"

static std::string FixEditorName( const std::string & orig )
{
	static const std::string names[] = {
		"ArcGIS",
		"bulk_upload",
		"Every Door Android",
		"Every Door iOS",
		"FindvejBot",
		"Go Kaart!!",
		"Go Map!!",
		"GpsMid",
		"iD",
		"iLOE",
		"JOSM",
		"Level0",
		"LINZ Address Import",
		"MAPS.ME android",
		"MAPS.ME ios",
		"MapComplete",
		"MapContrib",
		"Mapzen",
		"Merkaartor",
		"mumpot",
		"noteSolver_plugin",
		"nsr2osm",
		"OMaps",
		"OpeningHoursFixer",
		"OpenSeaMap",
		"Organic Maps",
		"Osm Go!",
		"osm2go",
		"Osmand",
		"OsmAnd",
		"osmapi",
		"Osmose",
		"OSM Conflator",
		"OSM Contributor",
		"OSM Localization Tool",
		"OsmHydrant",
		"osmtools",
		"Pic4Review",
		"POI+",
		"Potlatch",
		"RapiD",
		"reverter_plugin",
		"Rocketdata Conflator",
		"RoofMapper",
		"rosemary",
		"QGIS",
		"StreetComplete",
		"teryt2osm",
		"Vespucci",
	};
	const char * s = orig.c_str();
	for ( int i = 0; i < sizeof names/sizeof names[0]; ++i ) {
		const char * name = names[i].c_str();
		if ( memcmp( s, name, strlen(name) ) == 0 )
			return name;
	}
	return orig;
}


static bool IsIdent( char c )
{
	return isalnum( c ) || c == '_' || c == '?';
}

static bool GetKey( const char *& s, const char *& k, int & klen )
{
	const char * p = s;
	while ( isspace( *p ) )
		++p;
	if ( !isalpha( *p ) && *p != '?' && *p != '/' )
		return false;
	// get key
	k = p++;
	while ( IsIdent( *p ) )
		++p;
	klen = (int)(p - k);
	s = p;
	return true;
}

static bool GetValue( const char *& s, const char *& v, int & vlen )
{
	const char * p = s;
	while ( isspace( *p ))
		++p;
	if ( *p++ != '"' )
		return false;
	v = p;
	while ( *p && *p != '"' )
		++p;
	if ( *p == 0 )
		return false;
	vlen = (int)(p - v);
	++p; // closing quote

	s = p;
	return true;
}


static bool GetKeyValue( const char *& s, const char *& k, int & klen, const char *& v, int & vlen )
{
	const char * p = s;
	if ( !GetKey( p, k, klen ) )
		return false;

	// get =
	while (isspace( *p ))
		++p;
	if ( *p++ != '=' )
		return false;

	if ( !GetValue(p, v, vlen ) )
		return false;

	s = p;
	return true;
}

static bool GetOpeningBracket( const char *& s )
{
	while ( isspace(*s))
		++s;
	if ( *s == '<' ) {
		++s;
		return true;
	}
	return false;
}


static bool GetClosingBracket( const char *& s )
{
	while ( isspace( *s ) )
		++s;
	if ( (s[0] == '?' || s[0] == '/') && s[1] == '>' ) {
		s += 2;
		return true;
	}
	if ( *s == '>' ) {
		++s;
		return true;
	}
	return false;
}


static std::string UnescapeString( const char * s, int len )
{
	if ( memchr( s, '&', len ) == NULL ) {
		return std::string( s, len );
	}

	std::string dst;
	for ( int i = 0; i < len; ++i, ++s ) {
		if ( *s == '&' ) {
			if ( memcmp( s+1, "quot;", 5 ) == 0 ) {
				dst += '"';
			} else if ( memcmp( s+1, "apos;", 5) == 0 ) {
				dst += '\'';
			} else if ( memcmp( s+1, "lt;", 3) == 0 ) {
				dst += '<';
			} else if ( memcmp( s+1, "gt;", 3) == 0 ) {
				dst += '>';
			} else if ( memcmp( s+1, "amp;", 4) == 0 ) {
				dst += '&';
			}
		} else {
			dst += *s;
		}
	}
	return dst;
}

static bool IsEqual( const char * s1, int len, const char * s2 )
{
	return memcmp( s1, s2, len ) == 0 && s2[len] == 0;
}

// get xml initial header
bool IgnoreTag( const char *&s2, const char * tag )
{
	const char * s = s2;
	const char * key, *val;
	int klen, vlen;

	if ( !GetOpeningBracket(s))
		return false;
	if ( !GetKey( s, key, klen ) )
		return false;
	if ( !IsEqual( key, klen, tag ) )
		return false;
	while ( GetKeyValue( s, key, klen, val, vlen) )
		continue;
	if ( !GetClosingBracket( s ))
		return false;

	s2 = s;
	return true;
}

void ChangesetParser::addReader(ChangesetReader * reader)
{
	readers.push_back(reader);
}


ChangesetParser::ParseStatus ChangesetParser::parseChangeset( const char *& s, Changeset & changeset )
{
	const char *key, *val, *tag;
	int klen, vlen, taglen;

	changeset.min_lat = changeset.max_lat = changeset.min_lon = changeset.max_lon = 0;
	changeset.date = "";
	changeset.user = "";
	changeset.application = "";
	changeset.comment = "";
	changeset.ident = 0;
	changeset.uid = 0;
	changeset.editCount = 0;

	if ( !GetOpeningBracket( s ) )
		return PARSE_ERROR;
	if ( !GetKey( s, tag, taglen ) )
		return PARSE_ERROR;
	if ( !IsEqual( tag, taglen, "changeset" ) ) {
		if ( IsEqual( tag, taglen, "/osm" )) {
			if ( !GetClosingBracket( s )) {
				return PARSE_ERROR;
			}
			return PARSE_FINISHED;
		}
		return PARSE_ERROR;
	}

	// iterate over key/values
	while ( GetKeyValue( s, key, klen, val, vlen ) ) {
		if ( IsEqual( key, klen, "id" ) ) {
			changeset.ident = atol( val );
		} else if ( IsEqual( key, klen, "created_at" ) ) {
			changeset.date = UnescapeString( val, 10 );
		} else if ( IsEqual( key, klen, "user" ) ) {
			changeset.user = UnescapeString( val, vlen );
		} else if ( IsEqual( key, klen, "uid" ) ) {
			changeset.uid = atoi( val );
		} else if ( IsEqual( key, klen, "num_changes" ) ) {
			changeset.editCount = atoi( val );
		} else if ( IsEqual( key, klen, "min_lat" ) ) {
			changeset.min_lat = atof( val );
		} else if ( IsEqual( key, klen, "max_lat" ) ) {
			changeset.max_lat = atof( val );
		} else if ( IsEqual( key, klen, "min_lon" ) ) {
			changeset.min_lon = atof( val );
		} else if ( IsEqual( key, klen, "max_lon" ) ) {
			changeset.max_lon = atof( val );
		} else {
			// ignore
		}
	}
	if ( !GetClosingBracket( s ))
		return PARSE_ERROR;

	// If this is a 2005-era changeset then it won't contain any additional tags and we're done
	if ( s[-2] == '/' )
		return PARSE_SUCCESS;

	for (;;) {
		// iterate over tags
		if ( !GetOpeningBracket( s ) )
			return PARSE_ERROR;
		if ( !GetKey( s, tag, taglen ) )
			return PARSE_ERROR;

		if ( IsEqual( tag, taglen, "tag" )) {
			// iterate over key/values
			if ( GetKeyValue( s, key, klen, val, vlen ) ) {
				if ( IsEqual( key, klen, "k" ) && IsEqual( val, vlen, "created_by" )) {
					if ( GetKeyValue( s, key, klen, val, vlen)) {
						if ( IsEqual(key, klen, "v") ) {
							changeset.application = FixEditorName( UnescapeString( val, vlen ) );
						}
					}
				} else if ( IsEqual( key, klen, "k" ) && IsEqual( val, vlen, "comment" )) {
					if ( GetKeyValue( s, key, klen, val, vlen)) {
						if ( IsEqual(key, klen, "v") ) {
							changeset.comment = UnescapeString( val, vlen );
						}
					}
				}
			}
			while ( GetKeyValue( s, key, klen, val, vlen ) )
				continue;
			if ( !GetClosingBracket( s )) {
				return PARSE_ERROR;
			}

		} else if ( IsEqual( tag, taglen, "/changeset" )) {
			if ( !GetClosingBracket( s )) {
				return PARSE_ERROR;
			}
			return PARSE_SUCCESS;
		} else {
			return PARSE_ERROR;
		}
	}
}

bool ChangesetParser::parseXmlString( const char * xml, const char * startDate )
{
	// get xml initial header
	const char * s = xml;
	IgnoreTag( s, "?xml" );
	IgnoreTag( s, "osm" );
	IgnoreTag( s, "bound" );

	for ( auto reader = readers.begin(); reader != readers.end(); ++reader ) {
		(*reader)->initialize();
	}

	for (;;) {
		Changeset changeset;
		auto status = parseChangeset(s, changeset);
		if ( status == PARSE_SUCCESS ) {
			if ( changeset.date >= startDate ) {
				for ( auto reader = readers.begin(); reader != readers.end(); ++reader ) {
					(*reader)->handleChangeset( changeset );
				}
			}
		} else if ( status == PARSE_FINISHED ) {
			break;
		} else { // PARSE_ERROR
			return false;
		}
	}

	for ( auto reader = readers.begin(); reader != readers.end(); ++reader ) {
		(*reader)->finalizeChangesets();
	}

	return true;
};

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

bool ChangesetParser::parseXmlFile( std::string path, const char * startDate )
{
	if ( path.compare(path.length()-4, 4, ".bz2") == 0 ) {
		return false;
	}

	int fd = open( path.c_str(), O_RDONLY );
	if ( fd < 0 ) {
		perror("");
		return false;
	}
	struct stat statbuf = { 0 };
	fstat(fd,&statbuf);

	const void * mem = mmap(NULL, statbuf.st_size, PROT_READ, MAP_FILE|MAP_SHARED|MAP_NOCACHE, fd, 0);
	madvise( (void*)mem, statbuf.st_size, MADV_SEQUENTIAL );	// 222 without

	bool ok = parseXmlString( (const char *)mem, startDate );

	munmap( (void *)mem, statbuf.st_size);
	return ok;
}
