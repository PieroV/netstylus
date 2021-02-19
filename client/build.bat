@echo off

clang++ -std=c++17 -Wall -pedantic -fms-extensions -D_CRT_SECURE_NO_WARNINGS ^
	-I ../common ^
	window.cpp stylus_plugin.cpp stylus_manager.cpp ^
	-luser32 -lgdi32 -lole32 -lws2_32 -O3 -o netstylus.exe
