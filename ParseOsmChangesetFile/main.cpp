//
//  main.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce on 7/4/14.
//  Copyright (c) 2014 Bryce Cogswell. All rights reserved.
//


#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <algorithm>

#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "Countries.h"

double timestamp()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec + time.tv_usec * 1e-6;
}

double GreatCircleDistance(double lon1, double lat1, double lon2, double lat2)
{
	double EarthRadius = 6378137.0;

	// haversine formula
	double dlon = (lon2 - lon1) * M_PI / 180;
	double dlat = (lat2 - lat1) * M_PI / 180;
	double a = pow(sin(dlat / 2), 2) + cos(lat1 * M_PI / 180) * cos(lat2 * M_PI / 180) * pow(sin(dlon / 2), 2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));
	double meters = EarthRadius * c;
	return meters;
}

static std::string FixEditorName( const std::string & orig )
{
	static const std::string names[] = {
		"ArcGIS",
		"bulk_upload",
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
		if ( strncmp( s, name, strlen(name) ) == 0 )
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
			if ( strncmp( s+1, "quot;", 5 ) == 0 ) {
				dst += '"';
			} else if ( strncmp( s+1, "apos;", 5) == 0 ) {
				dst += '\'';
			} else if ( strncmp( s+1, "lt;", 3) == 0 ) {
				dst += '<';
			} else if ( strncmp( s+1, "gt;", 3) == 0 ) {
				dst += '>';
			} else if ( strncmp( s+1, "amp;", 4) == 0 ) {
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
	return strncmp( s1, s2, len ) == 0 && s2[len] == 0;
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

struct EditorInfo {
	std::set<std::string>	users;
	long					changesets;
	long					edits;
	long					large_areas;
};
typedef std::map<std::string,EditorInfo> EditorMap;	// map editor name to info
struct DateInfo {
	std::string date;
	EditorMap editors;
};
typedef std::list<DateInfo> DateList;

struct UserEditCount {
	long			changesetCount;
	long			editCount;
	long			changesetInChinaCount;
	long			editInChinaCount;
	std::string		date;
	long			lastChangesetId;
	UserEditCount(long changesetCount,long editCount) : editCount(editCount), changesetCount(changesetCount), changesetInChinaCount(0), editInChinaCount(0) {}
};

typedef std::pair<std::string,long> ChangesetCount;
struct pred {
	bool operator()(const ChangesetCount & a, const ChangesetCount & b) const
	{
		return a.second > b.second;
	}
};

bool parseXml( const char * s, const char * startDate )
{
	DateList	dateList;

	typedef std::map<std::string,UserEditCount>	PerEditorUserMap;	// map user-name to edit stats

	typedef std::map<std::string,PerEditorUserMap> PerEditorMap; // map editor name to stats
	PerEditorMap perEditorMap;
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Go Map!!",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Vespucci",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("StreetComplete",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("MapComplete",PerEditorUserMap()));

	typedef std::map<std::string,long> ChangesetCommentMap;
	ChangesetCommentMap changesetComments;

	const char *key, *val, *tag;
	int klen, vlen, taglen;

	// get xml initial header
	IgnoreTag( s, "?xml" );
	IgnoreTag( s, "osm" );
	IgnoreTag( s, "bound" );

	std::string changesetDate, changesetUser, changesetEditor, changesetComment;
	long changesetId = 0;
	int uid, editCount = 0;
	double min_lat = 0, max_lat = 0, min_lon = 0, max_lon = 0;

	for (;;) {
		// iterate over changesets
		if ( !GetOpeningBracket( s ) )
			break;

		if ( !GetKey( s, tag, taglen ) )
			return false;

		if ( IsEqual( tag, taglen, "changeset" ) ) {

			// save prevoius accumulated changeset info
			if ( changesetEditor.length() ) {
				if ( dateList.size() == 0 || dateList.back().date != changesetDate ) {
					dateList.push_back( DateInfo() );
					dateList.back().date = changesetDate;
				}
				DateInfo & pdate = dateList.back();
				std::pair<EditorMap::iterator,bool> result = pdate.editors.insert( std::pair<std::string,EditorInfo>(changesetEditor, EditorInfo()) );
				if ( result.second ) {
					// didn't already exist
				}
				EditorInfo & e = result.first->second;
				e.users.insert( changesetUser );
				e.edits += editCount;
				e.changesets += 1;
				if ( GreatCircleDistance(min_lon, min_lat, max_lon, max_lat) > 1000*1000.0 ) {
					e.large_areas += 1;
				}

				std::pair<ChangesetCommentMap::iterator,bool> comment = changesetComments.insert( std::pair<std::string,long>(changesetComment, 1) );
				if ( comment.second ) {
					// didn't already exist
				} else {
					comment.first->second++;
				}

				auto it = perEditorMap.find( changesetEditor );
				if ( it != perEditorMap.end() ) {
					PerEditorUserMap & perEditor = it->second;
					std::pair<PerEditorUserMap::iterator,bool> result = perEditor.insert(std::pair<std::string,UserEditCount>(changesetUser,UserEditCount(1,editCount)));
					UserEditCount & editsForUser = result.first->second;
					if ( !result.second ) {
						editsForUser.changesetCount += 1;
						editsForUser.editCount		 += editCount;
					}
					editsForUser.date	= changesetDate;
					editsForUser.lastChangesetId = changesetId;

#if 0
					if ( CountryContainsPoint( "China", min_lon, min_lat ) &&
						 CountryContainsPoint( "China", min_lon, max_lat ) &&
						 CountryContainsPoint( "China", max_lon, min_lat ) &&
						 CountryContainsPoint( "China", max_lon, max_lat ) )
					{
						printf( "China: %s %ld: %s -- %f, %f, %f, %f\n", changesetUser.c_str(), changesetId, changesetDate.c_str(), min_lat, max_lat, min_lon, max_lon );
						editsForUser.editInChinaCount += editCount;
						editsForUser.changesetInChinaCount += 1;
					}
#endif
				}
			}
			min_lat = max_lat = min_lon = max_lon = 0;
			changesetDate = "";
			changesetUser = "";
			changesetEditor = "";
			changesetComment = "";
			changesetId = 0;
			uid = 0;
			editCount = 0;

			// iterate over key/values
			while ( GetKeyValue( s, key, klen, val, vlen ) ) {
				if ( IsEqual( key, klen, "id" ) ) {
					changesetId = atol( val );
				} else if ( IsEqual( key, klen, "created_at" ) ) {
					static std::string prev = "";
					changesetDate = UnescapeString( val, 10 );
					if ( prev.length() >= 4 && prev[3] != changesetDate[3] ) {
						printf("%s\n",changesetDate.c_str());
					}
					prev = changesetDate;
				} else if ( IsEqual( key, klen, "user" ) ) {
					changesetUser = UnescapeString( val, vlen );
				} else if ( IsEqual( key, klen, "uid" ) ) {
					uid = atoi( val );
				} else if ( IsEqual( key, klen, "num_changes" ) ) {
					editCount = atoi( val );
				} else if ( IsEqual( key, klen, "min_lat" ) ) {
					min_lat = atof( val );
				} else if ( IsEqual( key, klen, "max_lat" ) ) {
					max_lat = atof( val );
				} else if ( IsEqual( key, klen, "min_lon" ) ) {
					min_lon = atof( val );
				} else if ( IsEqual( key, klen, "max_lon" ) ) {
					max_lon = atof( val );
				} else {
					// ignore
				}
			}
			if ( !GetClosingBracket( s )) {
				return false;
			}
		} else if ( IsEqual( tag, taglen, "tag" )) {
			// iterate over key/values
			if ( GetKeyValue( s, key, klen, val, vlen ) ) {
				if ( IsEqual( key, klen, "k" ) && IsEqual( val, vlen, "created_by" )) {
					if ( GetKeyValue( s, key, klen, val, vlen)) {
						if ( IsEqual(key, klen, "v") ) {
							changesetEditor = UnescapeString( val, vlen );
							changesetEditor = FixEditorName( changesetEditor );
						}
					}
				} else if ( IsEqual( key, klen, "k" ) && IsEqual( val, vlen, "comment" )) {
					if ( GetKeyValue( s, key, klen, val, vlen)) {
						if ( IsEqual(key, klen, "v") ) {
							changesetComment = UnescapeString( val, vlen );
						}
					}
				}
			}
			while ( GetKeyValue( s, key, klen, val, vlen ) )
				continue;
			if ( !GetClosingBracket( s )) {
				return false;
			}

		} else if ( IsEqual( tag, taglen, "/changeset" )) {
			if ( !GetClosingBracket( s )) {
				return false;
			}
		} else if ( IsEqual( tag, taglen, "/osm" )) {
			if ( !GetClosingBracket( s )) {
				return false;
			}
			break;
		} else {
			return false;
		}
	}

	printf("Start date = %s\n",startDate);
	printf("\n");

	typedef std::map<std::string,long>	EditorEditCountMap;
	EditorEditCountMap	editorEditCount;
	typedef std::map<std::string,long>	UserCountMap;	// for each editor count the number of daily users
	UserCountMap	userCount;
	typedef std::map<std::string,long>	LargeAreaMap;	// for each editor count the number of large changesets
	LargeAreaMap	largeAreaMap;
	long			dateCount = 0;
	long			goMapEdits = 0;
	for ( DateList::iterator date = dateList.begin(); date != dateList.end(); ++date ) {
		if ( date->date < startDate )
			continue;
		EditorMap & editors = date->editors;

		++dateCount;
		for ( EditorMap::iterator editor = editors.begin(); editor != editors.end(); ++editor ) {
			const std::string & name = editor->first;
			long count = editor->second.users.size();
			std::pair<UserCountMap::iterator,bool> result = userCount.insert(std::pair<std::string,long>(name,count));
			if ( !result.second ) {
				result.first->second += count;
			}
			if ( strncmp( name.c_str(), "Go Map", 6) == 0 ) {
				goMapEdits += editor->second.edits;
			}

			if ( editor->second.large_areas ) {
				std::pair<LargeAreaMap::iterator,bool> result = largeAreaMap.insert(std::pair<std::string,long>(name,1));
				if ( !result.second ) {
					result.first->second += 1;
				}
			}

			std::pair<EditorEditCountMap::iterator,bool> result2 = editorEditCount.insert(std::pair<std::string, long>(name,1));
			if ( !result2.second ) {
				result2.first->second += 1;
			}
		}
	}

	// print average number of daily users for each editor
	printf("\n");
	printf( "Sizes of edit sets:\n");
	for ( UserCountMap::iterator editor = userCount.begin(); editor != userCount.end(); ++editor ) {
		double rate = (double)editor->second / dateCount;
		if ( rate > 0.5 ) {
			printf( "%-30s %6.1f %6.1f\n", editor->first.c_str(), rate, editorEditCount.find(editor->first)->second/(double)dateCount );
		}
	}

	// print average number of daily users for each editor
	printf("\n");
	printf( "Average daily users for editors:\n");
	for ( UserCountMap::iterator editor = userCount.begin(); editor != userCount.end(); ++editor ) {
		double rate = (double)editor->second / dateCount;
		if ( rate > 0.5 ) {
			printf( "%-30s %6.1f %6.1f\n", editor->first.c_str(), rate, editorEditCount.find(editor->first)->second/(double)dateCount );
		}
	}

	// print large edit area counts
	printf("\n");
	printf( "Number of large changeset areas:\n");
	for ( LargeAreaMap::iterator editor = largeAreaMap.begin(); editor != largeAreaMap.end(); ++editor ) {
		long rate = editor->second;
		printf( "%-30s %6ld\n", editor->first.c_str(), rate );
	}

	// print number of edits each user of Go Map made
	for ( PerEditorMap::iterator it = perEditorMap.begin(); it != perEditorMap.end(); ++it ) {
		const char * editorName = it->first.c_str();
		PerEditorUserMap & perEditor = it->second;
		printf( "\n");
		printf( "%s users:\n", editorName);
		struct PerEditorUser {
			const std::string		name;
			UserEditCount			count;
			PerEditorUser( const std::string & name, UserEditCount count ) : name(name), count(count) {}
			bool operator < (const PerEditorUser & other) const	{ return count.date < other.count.date;	}
		};
		std::list<PerEditorUser> perEditorUserVector;
		long totalEdits = 0;
		long totalChangesets = 0;
		for ( PerEditorUserMap::iterator user = perEditor.begin(); user != perEditor.end(); ++user ) {
			perEditorUserVector.push_back(PerEditorUser(user->first,user->second));
			totalEdits += user->second.editCount;
			totalChangesets += user->second.changesetCount;
		}

		perEditorUserVector.sort( [](PerEditorUser const& a, PerEditorUser const& b) { return a.count.editCount > b.count.editCount; });
		while ( perEditorUserVector.size() > 100 ) {
			perEditorUserVector.pop_back();
		}
		// add totals
		perEditorUserVector.push_front(PerEditorUser("<Total>",UserEditCount(totalChangesets,totalEdits)));
		perEditorUserVector.front().count.date = "          ";

		printf( "   sets     edits        date        set\n");
		for ( std::list<PerEditorUser>::iterator user = perEditorUserVector.begin(); user != perEditorUserVector.end(); ++user ) {
			if ( user->count.editCount > 0 ) {
				printf( "%7ld %9ld  %s  %9ld  %s\n",
					   user->count.changesetCount,
					   user->count.editCount,
					   user->count.date.c_str(), user->count.lastChangesetId,
					   user->name.c_str() );
			}
		}
	}

	printf( "\n" );
	printf( "Go Map = %ld edits/day\n", goMapEdits/dateCount );

#if 0
	// print changeset comments
	std::vector<ChangesetCount> changesetVector;
	changesetVector.reserve( changesetComments.size());
	for ( ChangesetCommentMap::iterator c = changesetComments.begin(); c != changesetComments.end(); ++c ) {
		changesetVector.push_back(ChangesetCount(c->first,c->second));
	}
	std::sort(changesetVector.begin(),changesetVector.end(),pred());
	for ( int i = 0; i < 100; ++i ) {
		const ChangesetCount & c = changesetVector[ i ];
		double percent = 100.0 * c.second / changesetVector.size();
		printf("%9ld (%.6f) %s\n", c.second, percent, c.first.c_str());
	}
#endif
	return true;
}


bool parsePath( const std::string & path )
{
	if ( path.compare(path.length()-4, 4, ".bz2") == 0 ) {
		return false;
	}

	int fd = open( path.c_str(), O_RDONLY );
	if ( fd < 0 )
		return false;
	struct stat statbuf = { 0 };
	fstat(fd,&statbuf);

	const void * mem = mmap(NULL, statbuf.st_size, PROT_READ, MAP_FILE|MAP_SHARED|MAP_NOCACHE, fd, 0);
	//const char * e = strerror(errno);
	madvise( (void*)mem, statbuf.st_size, MADV_SEQUENTIAL );	// 222 without

	//bool ok = parseXml( (const char *)mem, "2014-10-04" );
	bool ok = parseXml( (const char *)mem, "2021-01-01" );

	munmap( (void *)mem, statbuf.st_size);
	return ok;
}


int main(int argc, const char * argv[])
{
	double time = timestamp();
	if ( argc == 2 ) {
		parsePath( argv[1] );
	} else {
		parsePath( "/tmp/cs.osm" );
	}
	time = timestamp() - time;
	printf( "total time = %f\n", time);
	return 0;
}
