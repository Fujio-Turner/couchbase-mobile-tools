//
// PutCommand.cc
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "fleece/FLExpert.h"
#include "c4BlobStore.h"
#include "c4DocumentTypes.h"
#include "c4Document+Fleece.h"
#include <algorithm>
#include <fstream>

using namespace std;
using namespace litecore;
using namespace fleece;


class PutCommand : public CBLiteCommand {
public:
    enum PutMode {kPut, kUpdate, kCreate, kDelete, kPurge};


    PutCommand(CBLiteTool &parent, PutMode mode)
    :CBLiteCommand(parent)
    ,_putMode(mode)
    { }


    void usage() override {
        switch (_putMode) {
            default:
                writeUsageCommand("put", true, "DOCID \"JSON\"");
                cerr <<
                "  Updates a document.\n"
                "    --attach <path> <name> [property] : Attach a file as a blob.\n"
                "        <path>: Path to the file to attach\n"
                "        <name>: Property name for the blob (default: 'blobs')\n"
                "        [property]: Optional nested property path (e.g., 'attachments.photo')\n"
                "    --create : Document must not exist\n"
                "    --delete : Deletes the document (and JSON is optional); same as `rm` subcommand\n"
                "    --purge  : Purges the document (deletes without leaving a tombstone)\n"
                "    --update : Document must already exist\n"
                "    -- : End of arguments (use if DOCID starts with '-')\n"
                "    " << it("DOCID") << " : Document ID\n"
                "    " << it("JSON") << " : Document body as JSON (JSON5 syntax allowed.)\n"
                ;
                break;
            case kDelete:
                writeUsageCommand("rm", true, "DOCID");
                cerr <<
                "  Deletes a document. (Same as `put --delete`)\n"
                "    --purge  : Purges the document (doesn't leave a tombstone)\n"
                "    " << it("DOCID") << " : Document ID\n"
                ;
                break;
        }
    }


    struct Attachment {
        string filePath;
        string propertyName;
    };

    void attachFlag() {
        string filePath = nextArg("file path");
        string propertyName = peekNextArg();
        if (propertyName.empty() || propertyName.starts_with('-'))
            propertyName = "blobs";
        else
            propertyName = nextArg("property name");
        _attachments.push_back({filePath, propertyName});
    }

    alloc_slice encodeJSONWithBlobs(slice jsonData, C4Error* c4error) {
        // Parse original JSON into a Fleece doc
        FLError flErr;
        FLDoc flDoc = FLDoc_FromJSON(FLSlice{jsonData.buf, jsonData.size}, &flErr);
        if (!flDoc) {
            if (c4error) { c4error->domain = LiteCoreDomain; c4error->code = kC4ErrorInvalidParameter; }
            return nullslice;
        }
        FLValue rootVal = FLDoc_GetRoot(flDoc);
        FLDict root = FLValue_AsDict(rootVal);
        if (!root) {
            FLDoc_Release(flDoc);
            if (c4error) { c4error->domain = LiteCoreDomain; c4error->code = kC4ErrorInvalidParameter; }
            return nullslice;
        }

        // Get blob store
        C4BlobStore* blobStore = c4db_getBlobStore(_db, c4error);
        if (!blobStore) {
            FLDoc_Release(flDoc);
            return nullslice;
        }

        // Begin new encoding with the DB's shared encoder
        FLEncoder enc = c4db_getSharedFleeceEncoder(_db);
        FLEncoder_BeginDict(enc, 0);

        // Copy existing properties
        FLDictIterator it;
        FLDictIterator_Begin(root, &it);
        for (FLValue v = FLDictIterator_GetValue(&it); v; v = FLDictIterator_GetValue(&it)) {
            FLString k = FLDictIterator_GetKeyString(&it);
            FLEncoder_WriteKey(enc, k);
            FLEncoder_WriteValue(enc, v);
            FLDictIterator_Next(&it);
        }

        // Attach blobs at top-level property names
        for (const auto& att : _attachments) {
            // Read file
            alloc_slice fileData = readFile(att.filePath);
            if (!fileData) {
                FLDoc_Release(flDoc);
                fail("Couldn't read attachment file: " + att.filePath);
            }

            // Create blob
            C4BlobKey blobKey;
            if (!c4blob_create(blobStore, (C4Slice)fileData, nullptr, &blobKey, c4error)) {
                FLDoc_Release(flDoc);
                return nullslice;
            }

            // Guess content type
            string contentType = "application/octet-stream";
            size_t dot = att.filePath.find_last_of('.');
            if (dot != string::npos) {
                string ext = att.filePath.substr(dot + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
                else if (ext == "png") contentType = "image/png";
                else if (ext == "gif") contentType = "image/gif";
                else if (ext == "pdf") contentType = "application/pdf";
                else if (ext == "txt") contentType = "text/plain";
                else if (ext == "json") contentType = "application/json";
            }

            // Blob metadata
            C4SliceResult digestStr = c4blob_keyToString(blobKey);

            // Write blob property
            FLString propKey = {(const char*)att.propertyName.data(), att.propertyName.size()};
            FLEncoder_WriteKey(enc, propKey);
            FLEncoder_BeginDict(enc, 4);

            FLEncoder_WriteKey(enc, FLStr(kC4ObjectTypeProperty));
            FLEncoder_WriteString(enc, FLStr(kC4ObjectType_Blob));

            FLEncoder_WriteKey(enc, FLStr(kC4BlobDigestProperty));
            FLEncoder_WriteString(enc, FLSlice{digestStr.buf, digestStr.size});

            FLEncoder_WriteKey(enc, FLStr("length"));
            FLEncoder_WriteUInt(enc, (uint64_t)fileData.size);

            FLEncoder_WriteKey(enc, FLStr("content_type"));
            FLString ctype = {(const char*)contentType.data(), contentType.size()};
            FLEncoder_WriteString(enc, ctype);

            FLEncoder_EndDict(enc);
            c4slice_free(digestStr);
        }

        FLEncoder_EndDict(enc);

        // Finish
        FLSliceResult out = FLEncoder_Finish(enc, &flErr);
        FLDoc_Release(flDoc);
        if (!out.buf) {
            if (c4error) { c4error->domain = LiteCoreDomain; c4error->code = kC4ErrorCorruptData; }
            return nullslice;
        }

        alloc_slice result(out);
        FLSliceResult_Release(out);
        return result;
    }

    void runSubcommand() override {
        // Read params:
        processFlags({
            {"--attach", [&]{attachFlag();}},
            {"--create", [&]{_putMode = kCreate;}},
            {"--update", [&]{_putMode = kUpdate;}},
            {"--delete", [&]{_putMode = kDelete;}},
            {"--purge",  [&]{_putMode = kPurge;}},
        });
        openWriteableDatabaseFromNextArg();

        string docID = nextArg("document ID");
        string json5;
        if (_putMode < kDelete)
            json5 = restOfInput("document body as JSON");
        endOfArgs();

        C4Error error;
        c4::Transaction t(_db);
        if (!t.begin(&error))
            fail("Couldn't open database transaction");

        c4::ref<C4Document> doc;
        bool existed = false;
        if (_putMode == kPurge) {
            if (!c4coll_purgeDoc(collection(), slice(docID), &error))
                fail("Couldn't purge document", error);
        } else {
            doc = c4coll_getDoc(collection(), slice(docID), false, kDocGetAll, &error);
            if (!doc)
                fail("Couldn't read document", error);
            existed = (doc->flags & kDocExists) != 0
            && (doc->selectedRev.flags & kRevDeleted) == 0;
            if (!existed && (_putMode == kUpdate || _putMode == kDelete)) {
                if (doc->flags & kDocExists)
                    fail("Document is already deleted");
                else
                    fail("Document doesn't exist");
            }
            if (existed && _putMode == kCreate)
                fail("Document already exists");

            alloc_slice body;
            if (_putMode != kDelete) {
                FLStringResult errMsg;
                alloc_slice json = FLJSON5_ToJSON(slice(json5), &errMsg, nullptr, nullptr);
                if (!json) {
                    string message = string(alloc_slice(errMsg));
                    FLSliceResult_Release(errMsg);
                    fail("Invalid JSON: " + message);
                }
                
                // If attachments are specified, modify the JSON to include them
                if (!_attachments.empty()) {
                    body = encodeJSONWithBlobs(json, &error);
                } else {
                    body = c4db_encodeJSON(_db, json, &error);
                }
                
                if (!body)
                    fail("Couldn't encode body", error);
            }

            // Set flags: deleted flag for deletes, attachment flag for blobs
            C4RevisionFlags flags = 0;
            if (_putMode == kDelete)
                flags |= kRevDeleted;
            if (!_attachments.empty())
                flags |= kRevHasAttachments;

            doc = c4doc_update(doc, body, flags, &error);
            if (!doc)
                fail("Couldn't save document", error);
        }
        if (!t.commit(&error))
            fail("Couldn't commit database transaction", error);

        const char *verb;
        if (_putMode == kPurge) {
            cout << "Purged `" << docID << "`\n";
        } else {
            if (_putMode == kDelete)
                verb = "Deleted";
            else if (existed)
                verb = "Updated";
            else
                verb = "Created";
            string revID = slice(doc->selectedRev.revID).asString();
            if (revID.size() > 10)
                revID.resize(10);
            cout << verb << " `" << docID << "`, new revision " << revID
                 << " (sequence " << doc->sequence << ")";
            if (!_attachments.empty())
                cout << " with " << _attachments.size() << " blob(s)";
            cout << "\n";
        }
    }

private:
    PutMode                 _putMode {kPut};
    vector<Attachment>      _attachments;
};


CBLiteCommand* newPutCommand(CBLiteTool &parent) {
    return new PutCommand(parent, PutCommand::kPut);
}

CBLiteCommand* newRmCommand(CBLiteTool &parent) {
    return new PutCommand(parent, PutCommand::kDelete);
}
