#!/bin/bash

protoc -I. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` -I ../proto/ ../proto/api.proto
protoc -I. --cpp_out=. -I ../proto/ ../proto/api.proto
