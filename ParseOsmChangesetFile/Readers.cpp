//
//  Readers.cpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/4/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <algorithm>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "Countries.h"
#include "ChangesetParser.hpp"
#include "Readers.hpp"

class EditorDailyUsersReader: public ChangesetReader {
	struct EditorInfo {
		long					changesets;
		long					edits;
		std::set<std::string>	usersPerDay;
		long					uniqueUsersPerDaySum;
	};
	typedef std::map<std::string,EditorInfo> EditorMap;	// map editor name to info
	EditorMap		editors;

	std::string		prevDate = "";
	long			dateCount = 0;

	void initialize() {
	}

	void process(const Changeset & changeset)
	{
		if ( changeset.date != prevDate ) {
			prevDate = changeset.date;
			for ( auto &editor_pair : editors ) {
				auto &editor = editor_pair.second;
				editor.uniqueUsersPerDaySum += editor.usersPerDay.size();
				editor.usersPerDay.clear();
			}
			++dateCount;
		}

		auto it = editors.find(changeset.application);
		if ( it == editors.end() ) {
			it = editors.insert( std::pair<std::string,EditorInfo>(changeset.application, EditorInfo()) ).first;
		}
		EditorInfo & e = it->second;
		e.usersPerDay.insert( changeset.user );
		e.edits += changeset.editCount;
		e.changesets += 1;
	}

	void finalize()
	{
		// print average number of unique daily users for each editor
		printf("\n");
		printf( "Average daily users and edits/user:\n");

		struct stats {
			double user_rate;
			double edit_rate;
			std::string	editor;
			bool operator<(const stats & a) const { return user_rate < a.user_rate; }
		};

		std::vector<stats>	list;
		for ( const auto &editor_pair: editors ) {
			const auto &editor = editor_pair.second;
			stats s = {
				(double)editor.uniqueUsersPerDaySum / dateCount,
				editor.edits / (double)editor.uniqueUsersPerDaySum,
				editor_pair.first
			};
			list.push_back(s);
		}
		std::sort( list.begin(), list.end() );
		std::reverse( list.begin(), list.end() );

		for ( const auto &item: list ) {
			if ( item.user_rate > 0.1 ) {
				printf( "%6.1f %12.1f  %s\n",
					   item.user_rate,
					   item.edit_rate,
					   item.editor.c_str() );
			}
		}
	}
};


class LargeAreaReader: public ChangesetReader {
	typedef std::map<std::string,long>	LargeAreaMap;	// for each editor count the number of large changesets
	LargeAreaMap	largeAreaMap;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		if ( GreatCircleDistance(changeset.min_lon, changeset.min_lat, changeset.max_lon, changeset.max_lat) > 1000*1000.0 ) {
			std::pair<LargeAreaMap::iterator,bool> result = largeAreaMap.insert(std::pair<std::string,long>(changeset.application,1));
			if ( !result.second ) {
				result.first->second += 1;
			}
		}
	}

	void finalize()
	{
		// print large edit area counts
		printf("\n");
		printf( "Number of large changeset areas:\n");
		for ( LargeAreaMap::iterator editor = largeAreaMap.begin(); editor != largeAreaMap.end(); ++editor ) {
			long rate = editor->second;
			printf( "%-30s %6ld\n", editor->first.c_str(), rate );
		}
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
};


class BiggestMappersByApp: public ChangesetReader {
	struct UserStats {
		long			changesetCount;
		long			editCount;
		std::string		lastDate;
		long			lastChangesetId;
		UserStats() : editCount(0), changesetCount(0) {}
	};
	typedef std::map<std::string,UserStats>	PerUserMap;	// map user-name to edit stats
	typedef std::map<std::string,PerUserMap> PerAppMap; // map editor name to stats
	PerAppMap perAppMap;

	void initialize() {
		perAppMap.insert(std::pair<std::string,PerUserMap>("Go Map!!",PerUserMap()));
		perAppMap.insert(std::pair<std::string,PerUserMap>("Vespucci",PerUserMap()));
		perAppMap.insert(std::pair<std::string,PerUserMap>("StreetComplete",PerUserMap()));
		perAppMap.insert(std::pair<std::string,PerUserMap>("MapComplete",PerUserMap()));
	}

	void process(const Changeset & changeset)
	{
		auto it = perAppMap.find( changeset.application );
		if ( it != perAppMap.end() ) {
			PerUserMap & userMap = it->second;

			auto it = userMap.find(changeset.application);
			if ( it == userMap.end() ) {
				it = userMap.insert( std::pair<std::string,UserStats>(changeset.user,UserStats()) ).first;
			}
			UserStats & userStats = it->second;
			userStats.changesetCount	+= 1;
			userStats.editCount			+= changeset.editCount;
			userStats.lastDate			= changeset.date;
			userStats.lastChangesetId = changeset.ident;
		}
	}

	void finalize()
	{
		const int TOP_COUNT = 20;

		// print number of edits each user of Go Map made
		for ( const auto &it: perAppMap ) {
			const char * editorName = it.first.c_str();
			const PerUserMap & perUserMap = it.second;
			printf( "\n");
			printf( "%s top %d prolific users:\n", editorName, TOP_COUNT);
			struct PerEditorUser {
				const std::string	name;
				UserStats			count;
				PerEditorUser( const std::string & name, UserStats count ) : name(name), count(count) {}
				bool operator < (const PerEditorUser & other) const	{ return count.lastDate < other.count.lastDate;	}
			};
			std::list<PerEditorUser> perEditorUserVector;
			long totalEdits = 0;
			long totalChangesets = 0;
			for ( const auto &user: perUserMap ) {
				perEditorUserVector.push_back(PerEditorUser(user.first,user.second));
				totalEdits += user.second.editCount;
				totalChangesets += user.second.changesetCount;
			}

			perEditorUserVector.sort( [](PerEditorUser const& a, PerEditorUser const& b) { return a.count.editCount > b.count.editCount; });
			while ( perEditorUserVector.size() > TOP_COUNT ) {
				perEditorUserVector.pop_back();
			}
			// add totals
			perEditorUserVector.push_front(PerEditorUser("<Total>",UserStats()));
			perEditorUserVector.front().count.lastDate = "          ";

			printf( "    edits    sets  most recent     last set   user\n");
			for ( const auto &user: perEditorUserVector ) {
				if ( user.count.editCount > 0 ) {
					printf( "%9ld %7ld   %s  %11ld   %s\n",
						   user.count.editCount,
						   user.count.changesetCount,
						   user.count.lastDate.c_str(),
						   user.count.lastChangesetId,
						   user.name.c_str() );
				}
			}
		}
	}
};


class GoMapInCountryReader: public ChangesetReader {
	const char * COUNTRY = "China";
	struct User {
		long	changesets;
		long	edits;
	};
	std::map<std::string,User>	users;
	void initialize() {}

	void process(const Changeset & changeset)
	{
		if ( changeset.application != "Go Map!!" )
			return;
		if ( CountryContainsPoint( COUNTRY, changeset.min_lon, changeset.min_lat ) &&
			CountryContainsPoint( COUNTRY, changeset.min_lon, changeset.max_lat ) &&
			CountryContainsPoint( COUNTRY, changeset.max_lon, changeset.min_lat ) &&
			CountryContainsPoint( COUNTRY, changeset.max_lon, changeset.max_lat ) )
		{
			auto it = users.insert(std::pair<std::string,User>(changeset.user,User())).first;
			it->second.edits += changeset.editCount;
			it->second.changesets += 1;
		}
	}

	void finalize()
	{
		struct UserInfo {
			long			edits;
			long			changesets;
			std::string		user;
			bool operator < (const UserInfo & other) const	{ return edits < other.edits;	}
		};
		std::vector<UserInfo>	list;

		for ( const auto &it: users ) {
			UserInfo info = {
				it.second.edits,
				it.second.changesets,
				it.first
			};
			list.push_back(info);
		}
		std::sort( list.begin(), list.end() );
		std::reverse( list.begin(), list.end() );

		printf( "\n");
		printf( "Top editors in %s:\n", COUNTRY);
		printf( "    edits    changesets    user\n");
		for ( const auto &user: list ) {
			printf( "%9ld   %7ld   %s\n",
				   user.edits, user.changesets, user.user.c_str());
		}
	}
};


// Shows which locale changesets are using
class GoMapLocaleReader: public ChangesetReader {
	std::map<std::string,long>	locales;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		if ( changeset.application == "Go Map!!" ) {
			auto it = locales.find(changeset.locale);
			if ( it == locales.end() ) {
				it = locales.insert( std::pair<std::string,long>(changeset.locale, 0) ).first;
			}
			++it->second;
		}
	}

	void finalize()
	{
		std::vector<std::pair<long, std::string>> list;
		for ( const auto &loc: locales ) {
			list.push_back(std::pair<long, std::string>(loc.second,loc.first));
		}
		std::sort(list.begin(),list.end());
		std::reverse(list.begin(),list.end());
		printf("\n");
		printf("Most common locales in Go Map!!\n");
		for ( const auto &loc: list ) {
			printf("%9ld  %s\n", loc.first, loc.second.c_str());
		}
	}
};


// Track the number of times each comment is used by StreetComplete users
std::set<std::string>	g_StreetCompleteComments;
class StreetCompleteReader: public ChangesetReader {
	std::map<std::string,long>	quests;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		if ( changeset.application == "StreetComplete" ) {
			g_StreetCompleteComments.insert( changeset.comment );
		}
		if ( changeset.quest_type.size() > 0 ) {
			auto it = quests.insert( std::pair<std::string,long>(changeset.quest_type, 0) ).first;
			it->second++;
		}
	}

	void finalize()
	{
		long total = 0;
		std::vector<std::pair<long,std::string>> scQuests;
		for (const auto & c: quests ) {
			scQuests.push_back(std::pair<long,std::string>(c.second,c.first));
			total += c.second;
		}
		std::sort( scQuests.begin(), scQuests.end() );
		std::reverse( scQuests.begin(), scQuests.end() );
		printf("\n");
		printf("StreetComplete quests:\n");
		double acc = 0.0;
		for ( const auto & c: scQuests ) {
			acc += c.first;
			printf("%9ld %.2f%% (%.2f%%) %s\n",
				   c.first,
				   100.0*c.first/total,
				   100.0*acc/total,
				   c.second.c_str());
		}
	}
};


// Track the most common changeset comments
class ChangesetCommentReader: public ChangesetReader {
	typedef std::map<std::string,long> ChangesetCommentMap;
	ChangesetCommentMap comments;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		auto it = comments.find(changeset.application);
		if ( it == comments.end() ) {
			it = comments.insert( std::pair<std::string,long>(changeset.comment, 0) ).first;
		}
		it->second++;
	}

	void finalize()
	{
		// print changeset comments
		typedef std::pair<long,std::string> Entry;
		std::vector<Entry> list;
		list.reserve( comments.size());
		for ( const auto & c: comments ) {
			if ( g_StreetCompleteComments.find( c.first ) != g_StreetCompleteComments.end() )
				continue;	// exclude comments from StreetComplete
			list.push_back(Entry(c.second,c.first));
		}
		std::sort(list.begin(),list.end());
		std::reverse(list.begin(),list.end());
		printf("\n");
		printf("Top 100 changeset comments:\n");
		for ( int i = 0; i < 100; ++i ) {
			const Entry & c = list[ i ];
			double percent = 100.0 * c.first / list.size();
			printf("%9ld (%.6f%%) \"%s\"\n", c.first, percent, c.second.c_str());
		}
	}
};

// Print the current date each time we reach a new year parsing the changeset file
class DatePrinterReader: public ChangesetReader {
	std::string prev = "";

	void initialize() {}
	void process(const Changeset & changeset)
	{
		if ( prev.length() == 0 || (prev[3] != changeset.date[3] && changeset.date >= "2010") ) {
			printf("%s\n",changeset.date.c_str());
		}
		prev = changeset.date;
	}
	void finalize()
	{
	}
};

//
class RetentionReader: public ChangesetReader {
	typedef std::map<std::string,long> EditorToCount;
	typedef std::map<std::string,EditorToCount> YearToEditor;	// year, editor, count
	YearToEditor	yearToEditor;		// year: editor: count

	void initialize() {}
	void process(const Changeset & changeset)
	{
		auto year = changeset.date.substr(0,4);
		auto editorToCountDict = yearToEditor.find(year);
		if ( editorToCountDict == yearToEditor.end() ) {
			editorToCountDict = yearToEditor.insert(std::pair<std::string,EditorToCount>(year,EditorToCount())).first;
		}
		auto editorMap = &editorToCountDict->second;
		auto editor = editorMap->find( changeset.application ) ;
		if ( editor == editorMap->end() ) {
			editor = editorMap->insert(std::pair<std::string,long>(changeset.application,0)).first;
		}
		++editor->second;
	}

	void finalize()
	{
		printf("\n");
		printf("Retention per editor\n");
		for ( const auto &year: yearToEditor ) {
			printf("year %s\n", year.first.c_str());
			auto eds = year.second;
			std::vector<std::pair<long, std::string>>	edVector;
			for ( const auto &ed: eds ) {
				edVector.push_back(std::pair<long,std::string>(ed.second,ed.first));
			}
			std::sort(edVector.begin(), edVector.end());
			std::reverse(edVector.begin(), edVector.end());
			int count = 0;
			for ( const auto &ed: edVector ) {
				printf("    %10ld:  %s\n", ed.first, ed.second.c_str() );
				if ( ++count == 10 )
					break;
			}
		}
	}
};

//
class EditsPerChangesetReader: public ChangesetReader {
	struct stats {
		int edits;
		int changesets;
		long lastChangeset;
	};
	typedef std::map<std::string,struct stats> Map;
	Map ratio;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		auto editor = ratio.find( changeset.application );
		if ( editor == ratio.end() ) {
			struct stats s = { 0, 0, 0 };
			editor = ratio.insert(std::pair<std::string, struct stats>(changeset.application,s)).first;
		}
		editor->second.changesets += 1;
		editor->second.edits += changeset.editCount;
		editor->second.lastChangeset = changeset.ident;

	}

	void finalize()
	{
		struct info {
			std::string	editor;
			double		ratio;
			int			changesets;
			long		lastChangeset;
			bool operator < (const struct info & other) const { return ratio < other.ratio; }
		};
		std::vector<struct info>	vec;
		for ( const auto &editor: ratio ) {
			struct info info = {
				editor.first,
				(double)editor.second.edits / editor.second.changesets,
				editor.second.changesets,
				editor.second.lastChangeset
			};
			vec.push_back(info);
		}
		std::sort(vec.begin(), vec.end());
		std::reverse(vec.begin(), vec.end() );

		printf("\n");
		printf("Edits/changeset per application\n");
		for ( const auto &editor: vec ) {
			if ( editor.changesets >= 100 ) {
				printf("%11.6f:  %s [%ld]\n", editor.ratio, editor.editor.c_str(), editor.lastChangeset );
			}
		}
	}
};

//
class EditStreaksReader: public ChangesetReader {
	typedef std::set<std::string>	SetOfUsers;
	typedef std::map<std::string,SetOfUsers>	UsersForDate;
	UsersForDate	usersForDate;

	void initialize() {}
	void process(const Changeset & changeset)
	{
		usersForDate[changeset.date].insert(changeset.user);
	}

	void finalize()
	{
		// convert the date map to a vector of dates and users
		std::vector<std::pair<std::string,SetOfUsers>>	dateList;
		for (const auto &it: usersForDate) {
			dateList.push_back(std::pair<std::string,SetOfUsers>(it.first,it.second));
		}
		std::sort( dateList.begin(), dateList.end() );

		struct editorStats {
			int prevDay;
			int dayCount;
			std::string startDate;
		};
		typedef std::map<std::string,struct editorStats> Editors;

		struct streakInfo {
			std::string		user;
			std::string		startDate;
			int				dayCount;
			bool operator < (const struct streakInfo & other) const { return dayCount < other.dayCount; }
		};
		std::vector<struct streakInfo>	streakList;

		std::string prevDay = "";
		int dayCounter = 0;
		Editors editors;

		for (const auto &date: dateList) {
			if ( date.first != prevDay ) {
				prevDay = date.first;
				++dayCounter;
			}
			for (const auto &user: date.second) {
				auto editor = editors.find( user );
				if ( editor == editors.end() ) {
					// new editor, so create a new entry for them
					struct editorStats s = { dayCounter, 1, date.first };
					editor = editors.insert(std::pair<std::string, struct editorStats>(user,s)).first;
				}
				if ( editor->second.prevDay == dayCounter ) {
					// another edit on the same day
				} else if ( editor->second.prevDay == dayCounter-1 ) {
					// they continued their streak
					editor->second.prevDay = dayCounter;
					editor->second.dayCount += 1;
				} else {
					// Their missed a day. Record their current streak if it's long enough to care
					if ( editor->second.dayCount > 100 ) {
						struct streakInfo s = { editor->first, editor->second.startDate, editor->second.dayCount };
						streakList.push_back( s );
					}
					// and start a new streak
					editor->second.prevDay = dayCounter;
					editor->second.dayCount = 1;
					editor->second.startDate = date.first;
				}
			}
		}

		// Handle any streaks in progress
		for ( const auto &editor: editors ) {
			if ( editor.second.prevDay >= dayCounter-1 ) {
				if ( editor.second.dayCount > 100 ) {
					struct streakInfo s = { editor.first, editor.second.startDate, editor.second.dayCount };
					streakList.push_back( s );
				}
			}
		}

		std::sort(streakList.begin(), streakList.end());
		std::reverse(streakList.begin(), streakList.end() );

		printf("\n");
		printf("Longest editing streaks:\n");
		printf("| Consecutive Days | First Day of Streak | User            |\n");
		printf("|------|------------|-----------------|\n");
		for ( int i = 0; i < 1000; ++i ) {
			const auto s = streakList[i];
			if ( s.dayCount == 0 )
				break;
			printf("|%11d| %s | %s |\n", s.dayCount, s.startDate.c_str(), s.user.c_str() );
		}
	}
};


std::vector<ChangesetReader *> getReaders()
{
	std::vector<ChangesetReader *>	readers;
//	readers.push_back(new DatePrinterReader());
	readers.push_back(new EditorDailyUsersReader());
	readers.push_back(new BiggestMappersByApp());
	readers.push_back(new StreetCompleteReader());
	readers.push_back(new ChangesetCommentReader());
	readers.push_back(new GoMapLocaleReader());
	readers.push_back(new GoMapInCountryReader());
	readers.push_back(new RetentionReader());
	readers.push_back(new EditsPerChangesetReader());
	readers.push_back(new EditStreaksReader());
	return readers;
}
