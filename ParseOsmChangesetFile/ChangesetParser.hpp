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

class Changeset {
public:
	std::string changesetDate, changesetUser, changesetEditor, changesetComment;
	long changesetId;
	int uid, editCount;
	double min_lat, max_lat, min_lon, max_lon;
};

class ChangesetParser {

public:
	typedef std::function<void (const Changeset &)> ChangesetCallback;
	ChangesetCallback callback;

	bool parseXml( const char * s, const char * startDate );
};

#endif /* parser_hpp */
