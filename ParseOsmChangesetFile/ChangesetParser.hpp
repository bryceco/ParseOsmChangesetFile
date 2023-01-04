//
//  parser.hpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/2/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#ifndef parser_hpp
#define parser_hpp

#include <stdio.h>
#include <vector>

// The data returned about each changeset
class Changeset {
public:
	std::string date, user, application, comment;
	long ident;
	int uid, editCount;
	double min_lat, max_lat, min_lon, max_lon;
};

// Virtual class that defines the callbacks from the parser
class ChangesetReader {
public:
	void virtual initialize() = 0;
	void virtual handleChangeset(const Changeset &) = 0;
	void virtual finalizeChangesets() = 0;
};

// The parser for changeset XML files
class ChangesetParser {
	enum ParseStatus { PARSE_SUCCESS, PARSE_ERROR, PARSE_FINISHED };
	enum ParseStatus parseChangeset( const char * &s, Changeset & changeset );
	const char * searchForStartDate( const char * xml, const char * end, const char * startDate );
	std::vector<ChangesetReader *> readers;
public:
	void addReader(ChangesetReader * reader);
	bool parseXmlString( const char * xml, long len, const char * startDate );
	bool parseXmlFile( std::string path, const char * startDate );
};

#endif /* parser_hpp */
