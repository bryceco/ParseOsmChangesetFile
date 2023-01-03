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
#include "ChangesetParser.hpp"

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
struct predChangesetCount {
	bool operator()(const ChangesetCount & a, const ChangesetCount & b) const
	{
		return a.second > b.second;
	}
};

typedef std::map<std::string,long>	EditorEditCountMap;
EditorEditCountMap	editorEditCount;
typedef std::map<std::string,long>	UserCountMap;	// for each editor count the number of daily users
UserCountMap	userCount;
typedef std::map<std::string,long>	LargeAreaMap;	// for each editor count the number of large changesets
LargeAreaMap	largeAreaMap;
long			dateCount = 0;
long			goMapEdits = 0;

DateList	dateList;

typedef std::map<std::string,UserEditCount>	PerEditorUserMap;	// map user-name to edit stats

typedef std::map<std::string,PerEditorUserMap> PerEditorMap; // map editor name to stats
PerEditorMap perEditorMap;

typedef std::map<std::string,long> ChangesetCommentMap;
ChangesetCommentMap changesetComments;

typedef std::map<std::string,long> StreetCompleteCommentMap;
StreetCompleteCommentMap streetCompleteComments;

void processChangeset( const Changeset & changeset )
{
	if ( dateList.size() == 0 || dateList.back().date != changeset.changesetDate ) {
		dateList.push_back( DateInfo() );
		dateList.back().date = changeset.changesetDate;
	}
	DateInfo & pdate = dateList.back();
	std::pair<EditorMap::iterator,bool> result = pdate.editors.insert( std::pair<std::string,EditorInfo>(changeset.changesetEditor, EditorInfo()) );
	if ( result.second ) {
		// didn't already exist
	}
	EditorInfo & e = result.first->second;
	e.users.insert( changeset.changesetUser );
	e.edits += changeset.editCount;
	e.changesets += 1;
	if ( GreatCircleDistance(changeset.min_lon, changeset.min_lat, changeset.max_lon, changeset.max_lat) > 1000*1000.0 ) {
		e.large_areas += 1;
	}

	std::pair<ChangesetCommentMap::iterator,bool> comment = changesetComments.insert( std::pair<std::string,long>(changeset.changesetComment, 1) );
	if ( comment.second ) {
		// didn't already exist
	} else {
		comment.first->second++;
	}

	if ( changeset.changesetEditor == "StreetComplete" ) {
		std::pair<StreetCompleteCommentMap::iterator,bool> scComment = streetCompleteComments.insert( std::pair<std::string,long>(changeset.changesetComment, 1) );
		if ( scComment.second ) {
			// didn't already exist
		} else {
			scComment.first->second++;
		}
	}

	auto it = perEditorMap.find( changeset.changesetEditor );
	if ( it != perEditorMap.end() ) {
		PerEditorUserMap & perEditor = it->second;
		std::pair<PerEditorUserMap::iterator,bool> result = perEditor.insert(std::pair<std::string,UserEditCount>(changeset.changesetUser,UserEditCount(1,changeset.editCount)));
		UserEditCount & editsForUser = result.first->second;
		if ( !result.second ) {
			editsForUser.changesetCount += 1;
			editsForUser.editCount		 += changeset.editCount;
		}
		editsForUser.date	= changeset.changesetDate;
		editsForUser.lastChangesetId = changeset.changesetId;

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

	static std::string prev = "";
	if ( prev.length() >= 4 && prev[3] != changeset.changesetDate[3] && changeset.changesetDate >= "2010" ) {
		printf("%s\n",changeset.changesetDate.c_str());
	}
	prev = changeset.changesetDate;
}

bool parseXml( const char * s, const char * startDate )
{
	printf("Start date = %s\n",startDate);
	printf("\n");

	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Go Map!!",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("Vespucci",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("StreetComplete",PerEditorUserMap()));
	perEditorMap.insert(std::pair<std::string,PerEditorUserMap>("MapComplete",PerEditorUserMap()));

	ChangesetParser * parser = new ChangesetParser();
	parser->callback = processChangeset;
	parser->parseXml( s, startDate );

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

		printf( "   sets     edits        date          set\n");
		for ( std::list<PerEditorUser>::iterator user = perEditorUserVector.begin(); user != perEditorUserVector.end(); ++user ) {
			if ( user->count.editCount > 0 ) {
				printf( "%7ld %9ld  %s  %11ld  %s\n",
					   user->count.changesetCount,
					   user->count.editCount,
					   user->count.date.c_str(),
					   user->count.lastChangesetId,
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

	long scTotal = 0;
	std::vector<std::pair<long,std::string>> scComments;
	for ( StreetCompleteCommentMap::iterator c = streetCompleteComments.begin(); c!= streetCompleteComments.end(); ++c ) {
		scComments.push_back(std::pair<long,std::string>(c->second,c->first));
		scTotal += c->second;
	}
	std::sort( scComments.begin(), scComments.end() );
	std::reverse( scComments.begin(), scComments.end() );
	printf("");
	printf("StreetComplete comments");
	for ( auto c = scComments.begin(); c!= scComments.end(); ++c ) {
		printf("%.6f%% %s\n", 100.0*c->first/scTotal, c->second.c_str());
	}

	return true;
}


bool parsePath( const std::string & path )
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
