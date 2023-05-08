//
//  parser.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/2/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#include <map>
#include <string>

#include "ChangesetParser.hpp"

#define PRINT_UNUSED_TAGS	0

static std::string FixEditorName( const std::string & origString )
{
	static const char * const names[] = {
		"Go Map!!",
		"Paint The Town Red",
		"Every Door",
		"MAPS.ME",
		"OsmAnd",
		"Organic Maps",
		"OMaps",
		"StreetComplete",
	};

	// Some apps needs to be truncated earlier than the version number
	const char * orig = origString.c_str();
	for ( int i = 0; i < sizeof names/sizeof names[0]; ++i ) {
		const char * name = names[i];
		if ( memcmp( orig, name, strlen(name) ) == 0 )
			return name;
	}

	// truncate at version number:
	//    ' 1'
	//    '/1'
	//    '-1'
	//    ' v1'
	for ( const char * s = orig+1; *s; ++s ) {
		if ( s[0] == ' ' || (s[0] == '/' && s[-1] != '/') || s[0] == '-' ) {
			if ( isdigit(s[1]) || (s[1] == 'v' && isdigit(s[2])) ) {
				return origString.substr(0,s-orig);
			}
		}
	}

	return origString;
}


static bool IsIdent( char c )
{
	return isalnum( c ) || c == '_' || c == '?';
}

// Parses a key: changeset
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

// Parses a quoted string: "JOSM 1.2"
static bool GetValue( const char *& s, const char *& v, int & vlen )
{
	const char * p = s;
	while ( isspace( *p ))
		++p;
	if ( *p++ != '"' )
		return false;
	v = p;
	while ( *p != '"' )
		++p;
	vlen = (int)(p - v);
	++p; // closing quote

	s = p;
	return true;
}

// Parses strings in the form:
// 		k="created_by"
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
	if ( (s[0] == '/' || s[0] == '?') && s[1] == '>' ) {
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
	const char * end = s + len;
	while ( s < end ) {
		if ( *s == '&' ) {
			if ( memcmp( s+1, "quot;", 5 ) == 0 ) {
				dst += '"';
				s += 5;
			} else if ( memcmp( s+1, "apos;", 5) == 0 ) {
				dst += '\'';
				s += 5;
			} else if ( memcmp( s+1, "lt;", 3) == 0 ) {
				dst += '<';
				s += 3;
			} else if ( memcmp( s+1, "gt;", 3) == 0 ) {
				dst += '>';
				s += 3;
			} else if ( memcmp( s+1, "amp;", 4) == 0 ) {
				dst += '&';
				s += 4;
			} else {
				dst += *s;
			}
		} else {
			dst += *s;
		}
		++s;
	}
	return dst;
}

static bool IsEqual( const char * s1, int len, const char * s2 )
{
	return memcmp( s1, s2, len ) == 0 && s2[len] == 0;
}

static bool IgnoreTag( const char *&s2, const char * tag )
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

#if PRINT_UNUSED_TAGS
std::map<std::string,long>	extraTags;
static void extraTag(const char * key, int klen)
{
	auto s = UnescapeString(key, klen);
	auto it = extraTags.insert(std::pair<std::string,long>(s,0)).first;
	it->second++;
}
#endif

ChangesetParser::ParseStatus ChangesetParser::parseChangeset( const char *& s, Changeset & changeset )
{
	const char *key, *val, *tag;
	int klen, vlen, taglen;

	changeset.min_lat = changeset.max_lat = changeset.min_lon = changeset.max_lon = 0.0;
	changeset.date = "";
	changeset.user = "";
	changeset.application = "";
	changeset.comment = "";
	changeset.ident = 0;
	changeset.uid = 0;
	changeset.editCount = 0;
	changeset.quest_type = "";

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
#if PRINT_UNUSED_TAGS
			extraTag(key, klen);
#endif
		}
	}
	if ( !GetClosingBracket( s ))
		return PARSE_ERROR;

	// If this is a 2005-era changeset then it won't contain any additional tags and we're done
	if ( s[-2] == '/' )
		return PARSE_SUCCESS;

	// iterate over tags
	for (;;) {
		// <tag k="created_by" v="JOSM"/>
		if ( !GetOpeningBracket( s ) )
			return PARSE_ERROR;
		if ( !GetKey( s, tag, taglen ) )
			return PARSE_ERROR;

		if ( IsEqual( tag, taglen, "tag" )) {
			if ( !GetKeyValue( s, key, klen, val, vlen ) )
				return PARSE_ERROR;
			if ( !IsEqual( key, klen, "k" ) )
				return PARSE_ERROR;
			if ( IsEqual( val, vlen, "created_by" )) {
				if ( GetKeyValue( s, key, klen, val, vlen)) {
					if ( IsEqual(key, klen, "v") ) {
						changeset.applicationRaw = UnescapeString( val, vlen );
						changeset.application = FixEditorName( changeset.applicationRaw );
					}
				}
			} else if ( IsEqual( val, vlen, "comment" )) {
				if ( GetKeyValue( s, key, klen, val, vlen)) {
					if ( IsEqual(key, klen, "v") ) {
						changeset.comment = UnescapeString( val, vlen );
					}
				}
			} else if ( IsEqual( val, vlen, "locale" )) {
				if ( GetKeyValue( s, key, klen, val, vlen)) {
					if ( IsEqual(key, klen, "v") ) {
						changeset.locale = UnescapeString( val, vlen );
					}
				}
			} else if ( IsEqual( val, vlen, "StreetComplete:quest_type" )) {
				if ( GetKeyValue( s, key, klen, val, vlen)) {
					if ( IsEqual(key, klen, "v") ) {
						changeset.quest_type = UnescapeString( val, vlen );
					}
				}

			} else {
				// some tag we don't care about, but we need to consume it's value:
				// 		changesets_count, host, imagery_used, locale
#if PRINT_UNUSED_TAGS
				extraTag(val, vlen);
#endif
				GetKeyValue( s, key, klen, val, vlen );
			}
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

// binary search for first changeset for startDate
const char * ChangesetParser::searchForStartDate( const char * start, const char * end,
												 std::string targetDate )
{
	// pick midpoint
	const char * mid = start + (end - start)/2;
	// scan forward for changeset
	const char * key = "<changeset ";
	size_t keylen = strlen(key);
	while ( mid+keylen < end && memcmp(mid,key,keylen)!=0 ) {
		++mid;
	}
	if ( mid+keylen >= end )
		return start;
	// get the changeset at the midpoint
	Changeset cs;
	const char * tmpMid = mid;
	if ( parseChangeset(tmpMid, cs) != PARSE_SUCCESS ) {
		// give up
		return start;
	}
	if ( cs.date < targetDate ) {
		return searchForStartDate(mid, end, targetDate);
	} else {
		return searchForStartDate(start, mid, targetDate);
	}
}

bool ChangesetParser::parseXmlString( const char * xml, long len, std::string startDate )
{
	// get xml initial header
	const char * s = xml;
	IgnoreTag( s, "?xml" );
	IgnoreTag( s, "osm" );
	IgnoreTag( s, "bound" );

	for ( auto reader: readers ) {
		reader->initialize();
	}

	// if a start date is defined then binary search for the changeset at or before it
	if ( startDate.size() > 0 ) {
		s = searchForStartDate( s, xml+len, startDate );
	}

	// iterate over all changesets
	for (;;) {
		Changeset changeset;
		auto status = parseChangeset(s, changeset);
		if ( status == PARSE_SUCCESS ) {
			if ( changeset.date >= startDate ) {
				for ( auto reader = readers.begin(); reader != readers.end(); ++reader ) {
					(*reader)->process( changeset );
				}
			}
		} else if ( status == PARSE_FINISHED ) {
			break;
		} else { // PARSE_ERROR
			return false;
		}
	}

	for ( auto reader: readers ) {
		reader->finalize();
	}

#if PRINT_UNUSED_TAGS
	// Show counts of tags we ignored
	printf("\n");
	printf("Unused tags:\n");
	std::vector<std::pair<long,std::string>>	extraCounts;
	for ( const auto &it: extraTags ) {
		extraCounts.push_back( std::pair<long,std::string>(it.second,it.first) );
	}
	std::sort( extraCounts.begin(), extraCounts.end() );
	std::reverse( extraCounts.begin(), extraCounts.end() );
	for ( const auto &it : extraCounts ) {
		printf("%9ld  %s\n", it.first, it.second.c_str());
	}
	printf("\n");
#endif

	return true;
};

void ChangesetParser::addReader(ChangesetReader * reader)
{
	readers.push_back(reader);
}

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

bool ChangesetParser::parseXmlFile( std::string path, std::string startDate )
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
	madvise( (void*)mem, statbuf.st_size, MADV_SEQUENTIAL );

	bool ok = parseXmlString( (const char *)mem, statbuf.st_size, startDate );

	munmap( (void *)mem, statbuf.st_size);
	return ok;
}
