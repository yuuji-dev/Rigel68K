#!/bin/bash

g++ main.cpp instlook.cpp directive.cpp build.cpp globals.cpp movem.cpp macro.cpp symbol.cpp object.cpp opparse.cpp eval.cpp error.cpp assembler.cpp codegen.cpp instructionstable.cpp structured.cpp  listing.cpp -o Rigel68K

g++ main.cpp instlook.cpp directive.cpp build.cpp globals.cpp movem.cpp macro.cpp symbol.cpp object.cpp opparse.cpp eval.cpp error.cpp assembler.cpp codegen.cpp instructionstable.cpp structured.cpp  listing.cpp -m32 -o  Rigel68K_32