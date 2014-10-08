/*

This Source Code Form is copyright of Yorkshire, Inc.
Copyright © 2014 Yorkshire, Inc,
Guiyang, Guizhou, China

This Source Code Form is copyright of 51Degrees Mobile Experts Limited.
Copyright © 2014 51Degrees Mobile Experts Limited, 5 Charlotte Close,
Caversham, Reading, Berkshire, United Kingdom RG4 7BY

This Source Code Form is the subject of the following patent
applications, owned by 51Degrees Mobile Experts Limited of 5 Charlotte
Close, Caversham, Reading, Berkshire, United Kingdom RG4 7BY:
European Patent Application No. 13192291.6; and
United States Patent Application Nos. 14/085,223 and 14/085,301.

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0.

If a copy of the MPL was not distributed with this file, You can obtain
one at http://mozilla.org/MPL/2.0/.

This Source Code Form is “Incompatible With Secondary Licenses”, as
defined by the Mozilla Public License, v. 2.0.

*/

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <node.h>
#include <v8.h>
#include <nan.h>
#include "pattern.h"

#define BUFFER_LENGTH 50000

#ifdef _MSC_VER
#define _INTPTR 0
#endif

using namespace v8;

PatternParser::PatternParser(char * filename, char * required_properties) {
  data_set = (DataSet *) malloc(sizeof(DataSet));
  init_result = initWithPropertyString(filename, data_set, required_properties);
}

PatternParser::~PatternParser() {
}

void PatternParser::Init(Handle<Object> target) {
  NanScope();
  Local<FunctionTemplate> t = NanNew<FunctionTemplate>(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  NODE_SET_PROTOTYPE_METHOD(t, "parse", Parse);
  target->Set(NanNew<v8::String>("PatternParser"), t->GetFunction());
}

NAN_METHOD(PatternParser::New) {
  NanScope();
  char *filename;
  char *required_properties;

  v8::String::Utf8Value v8_filename(args[0]->ToString());
  v8::String::Utf8Value v8_properties(args[1]->ToString());
  filename = *v8_filename;
  required_properties = *v8_properties;

  PatternParser *parser = new PatternParser(filename, required_properties);
  parser->Wrap(args.This());

  switch(parser->init_result) {
    case DATA_SET_INIT_STATUS_INSUFFICIENT_MEMORY:
      return NanThrowError("Insufficient memory");
    case DATA_SET_INIT_STATUS_CORRUPT_DATA:
      return NanThrowError("Device data file is corrupted");
    case DATA_SET_INIT_STATUS_INCORRECT_VERSION:
      return NanThrowError("Device data file is not correct");
    case DATA_SET_INIT_STATUS_FILE_NOT_FOUND:
      return NanThrowError("Device data file not found");
    default:
      NanReturnValue(args.This());
  }
}

NAN_METHOD(PatternParser::Parse) {
  NanScope();

  PatternParser *parser = ObjectWrap::Unwrap<PatternParser>(args.This());

  char output[BUFFER_LENGTH];
  char *input = NULL;
  
  Local<Object> result = NanNew<Object>();
  v8::String::Utf8Value v8_input(args[0]->ToString());
  input = *v8_input;

  Workset *ws = createWorkset(parser->data_set);
  ws->input = input;
  match(ws, ws->input);
  if (ws->profileCount > 0) {

    // build json
    int32_t propertyIndex, valueIndex, profileIndex;
    int idSize = ws->profileCount * 5 + (ws->profileCount - 1) + 1;
    char ids[idSize];
    char *pos = ids;
    for (profileIndex = 0; profileIndex < ws->profileCount; profileIndex++) {
      if (profileIndex < ws->profileCount - 1)
        pos += snprintf(pos, idSize, "%d-", (*(ws->profiles + profileIndex))->profileId);
      else
        pos += snprintf(pos, idSize, "%d", (*(ws->profiles + profileIndex))->profileId);
    }
    result->Set(NanNew<v8::String>("Id"), NanNew<v8::String>(ids));

    for (propertyIndex = 0; 
      propertyIndex < ws->dataSet->requiredPropertyCount; 
      propertyIndex++) {

      if (setValues(ws, propertyIndex) <= 0)
        break;

      const char *key = getPropertyName(ws->dataSet, 
        *(ws->dataSet->requiredProperties + propertyIndex));

      if (ws->valuesCount == 1) {
        const char *val = getValueName(ws->dataSet, *(ws->values));
        if (strcmp(val, "True") == 0)
          result->Set(NanNew<v8::String>(key), NanTrue());
        else if (strcmp(val, "False") == 0)
          result->Set(NanNew<v8::String>(key), NanFalse());
        else
          result->Set(NanNew<v8::String>(key), NanNew<v8::String>(val));
      } else {
        Local<Array> vals = NanNew<Array>(ws->valuesCount - 1);
        for (valueIndex = 0; valueIndex < ws->valuesCount; valueIndex++) {
          const char *val = getValueName(ws->dataSet, *(ws->values + valueIndex));
          vals->Set(valueIndex, NanNew<v8::String>(val));
        }
        result->Set(NanNew<v8::String>(key), vals);
      }
    }

    Local<Object> meta = NanNew<Object>();
    meta->Set(NanNew<v8::String>("difference"), NanNew<v8::Integer>(ws->difference));
    meta->Set(NanNew<v8::String>("method"), NanNew<v8::Integer>(ws->method));
    meta->Set(NanNew<v8::String>("rootNodesEvaluated"), NanNew<v8::Integer>(ws->rootNodesEvaluated));
    meta->Set(NanNew<v8::String>("nodesEvaluated"), NanNew<v8::Integer>(ws->nodesEvaluated));
    meta->Set(NanNew<v8::String>("stringsRead"), NanNew<v8::Integer>(ws->stringsRead));
    meta->Set(NanNew<v8::String>("signaturesRead"), NanNew<v8::Integer>(ws->signaturesRead));
    meta->Set(NanNew<v8::String>("signaturesCompared"), NanNew<v8::Integer>(ws->signaturesCompared));
    meta->Set(NanNew<v8::String>("closestSignatures"), NanNew<v8::Integer>(ws->closestSignatures));
    result->Set(NanNew<v8::String>("__meta__"), meta);

    // XXX: call without error
    // freeWorkset(ws);
  } else {
    printf("null\n");
  }
  NanReturnValue(result);
}


NODE_MODULE(pattern, PatternParser::Init)
