# The MIT License (MIT)
#
# Copyright (c) 2013 Téo .L
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Dossier de travail
BASE_DIR = ../../../

# Compilateur
#CXX = g++-3.4 # maximise la compatibilité
#CXX = g++-4.1 # disponible sur les distri récentes
CXX = g++

# Nom du fichier binaire de sortie
BINARY_NAME = triggerbot_public.so

# Dossier de sortie du fichier binaire
BINARY_DIR = .

# Code source du SDK de VALVE
SDK_SRC_DIR = $(BASE_DIR)
SDK_PUBLIC_DIR = $(SDK_SRC_DIR)/public
SDK_TIER0_DIR = $(SDK_SRC_DIR)/public/tier0
SDK_TIER1_DIR = $(SDK_SRC_DIR)/tier1
SDK_GAME_DIR = $(SDK_SRC_DIR)/game
SDK_GAMESHARED_DIR = $(SDK_GAME_DIR)/shared
SDK_GAMESERVER_DIR = $(SDK_GAME_DIR)/server

# Dossiers de sortie
RELEASE_DIR = Release/linux
DEBUG_DIR = Debug/linux

# Dossier contenant les librairies dynamiques
#SRCDS_DIR = ./
SRCDS_BIN_DIR = ./bin

# Dossier contenant les librairies statiques
SRCDS_A_DIR = $(SDK_SRC_DIR)/lib/linux

# Paramètres du compilateur
ARCH_CFLAGS = -mtune=i486 -march=pentium  -mmmx
USER_CFLAGS = -DTIXML_USE_TICPP
BASE_CFLAGS =   -msse \
                                -fpermissive \
                                -D_LINUX \
                                -DNDEBUG \
                                -Dstricmp=strcasecmp \
                                -D_stricmp=strcasecmp \
                                -D_strnicmp=strncasecmp \
                                -Dstrnicmp=strncasecmp \
                                -D_snprintf=snprintf \
                                -D_vsnprintf=vsnprintf \
                                -D_alloca=alloca \
                                -Dstrcmpi=strcasecmp \
                                -fPIC \
                                -Wno-deprecated \
                                -msse 
OPT_FLAGS = -O3 -funroll-loops -s -pipe
DEBUG_FLAGS = -g -ggdb3 -O0 -D_DEBUG -DDEV                       
#DEBUG_FLAGS = $(OPT_FLAGS)
# Fichiers à compiler
SRC= $(wildcard *.cpp) 
# $(wildcard */*.cpp) $(wildcard */*/*.cpp)                        

# Fichiers à lier
LINK_SO =       $(SRCDS_BIN_DIR)/libtier0.so                    
LINK_A =        $(SRCDS_A_DIR)/tier1_i486.a \
				$(SRCDS_A_DIR)/mathlib_i486.a

LINK = -lm -ldl $(LINK_A) $(LINK_SO)

# Dossiers des fichiers inclus
INCLUDE =       -I. \
                        -I$(SDK_PUBLIC_DIR) \
                        -I$(SDK_PUBLIC_DIR)/engine \
                        -I$(SDK_PUBLIC_DIR)/tier0 \
                        -I$(SDK_PUBLIC_DIR)/tier1 \
                        -I$(SDK_PUBLIC_DIR)/vstdlib \
                        -I$(SDK_PUBLIC_DIR)/game/server \
                        -I$(SDK_SRC_DIR)/tier1 \
                        -I$(SDK_SRC_DIR)/game \
                        -I$(SDK_SRC_DIR)/game/server \
                        -I$(SDK_SRC_DIR)/game/shared
                        

# Règles de compilation

ifeq "$(DEBUG)" "false"
	BIN_DIR = $(RELEASE_DIR)
	CFLAGS = $(OPT_FLAGS)
else
	BIN_DIR = $(DEBUG_DIR)
	CFLAGS = $(DEBUG_FLAGS)
endif
CFLAGS += $(USER_CFLAGS) $(BASE_CFLAGS) $(ARCH_CFLAGS)

OBJECTS := $(SRC:%.cpp=$(BIN_DIR)/%.o)

compile_object = \
	@mkdir -p $(2); \
	echo "$(1) => $(3)"; \
	$(CXX) $(INCLUDE) $(CFLAGS) -o $(3) -c $(1) 2> "/sdk/error_triggerbot.txt";

$(BIN_DIR)/%.o: %.cpp %.h
	$(call compile_object, $<, $(@D), $@)

$(BIN_DIR)/%.o: %.cpp
	$(call compile_object, $<, $(@D), $@)

all: $(OBJECTS)
	@$(CXX) $(INCLUDE) $(CFLAGS) $(OBJECTS) $(LINK) -shared -o $(BINARY_DIR)/$(BINARY_NAME)
        
release:
	@$(MAKE) all DEBUG=false
	
remake:
	@$(MAKE) clean
	@$(MAKE)

clean:
	@rm -rf $(RELEASE_DIR)
	@rm -rf $(DEBUG_DIR)
	@rm -rf $(BINARY_DIR)/$(BINARY_NAME)
        
.PHONY: clean
