//
//  Readers.hpp
//  ParseOsmChangesetFile
//
//  Created by Bryce Cogswell on 1/4/23.
//  Copyright © 2023 Bryce Cogswell. All rights reserved.
//

#ifndef Readers_hpp
#define Readers_hpp

#include "ChangesetParser.hpp"
#include <vector>

std::vector<ChangesetReader *> getReaders();

#endif /* Readers_hpp */
