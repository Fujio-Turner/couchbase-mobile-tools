//
// RmCollCommand.cc
//
// Copyright © 2025 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include "CBLiteCommand.hh"
#include <algorithm>


using namespace std;
using namespace litecore;


class RmCollCommand : public CBLiteCommand {
public:

    RmCollCommand(CBLiteTool &parent)
    :CBLiteCommand(parent)
    { }


    void usage() override {
        writeUsageCommand("rmcoll", true, "COLLECTION_PATH");
        cerr <<
        "  Deletes a collection.\n"
        "    COLLECTION_PATH can be either a collection name in the default scope,\n"
        "    or of the form <scope_name>/<collection_name>\n"
        "    WARNING: This permanently deletes all documents in the collection!\n"
        ;
    }


    void runSubcommand() override {
        openWriteableDatabaseFromNextArg();
        do {
            string input = nextArg("collection name");
            auto [scope, coll] = getCollectionPath(input);

            // Check if collection exists
            C4CollectionSpec spec {slice(coll), slice(scope)};
            C4Error error;
            c4::ref<C4Collection> collection = c4db_getCollection(_db, spec, &error);
            
            if (!collection) {
                fail("Collection '" + scope + "/" + coll + "' does not exist");
            }

            // Delete the collection
            if (!c4db_deleteCollection(_db, spec, &error))
                fail("Couldn't delete collection '" + input + "'", error);
            
            cout << "Deleted collection '" << scope << "/" << coll << "'.\n";
        } while (hasArgs());
    }
};


CBLiteCommand* newRmCollCommand(CBLiteTool &parent) {
    return new RmCollCommand(parent);
}
