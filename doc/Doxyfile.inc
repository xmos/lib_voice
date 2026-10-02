# This file provides overrides to the Doxyfile configuration

PROJECT_NAME = lib_voice
PROJECT_BRIEF = "Voice processing library"

# Alternating architecture
INPUT += ../lib_voice/api/alt_arch.h

# AEC module
INPUT += ../lib_voice/api/aec

# NS module
INPUT += ../lib_voice/api/ns

# AGC module
INPUT += ../lib_voice/api/agc

# ADEC module
INPUT += ../lib_voice/api/adec

# IC module
INPUT += ../lib_voice/api/ic

# VNR module
INPUT += ../lib_voice/api/vnr

# Stage1
INPUT += ../lib_voice/api/stage1

PREDEFINED             = C_API= EXTERN_C= DWORD_ALIGNED=

USE_MATHJAX            = YES
MATHJAX_FORMAT         = HTML-CSS
MATHJAX_RELPATH        = https://cdnjs.cloudflare.com/ajax/libs/mathjax/2.7.5/

# Make short MATHJAX sequences prettier
ALIASES += math{1}="@f$\1@f$"
