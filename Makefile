CXX := clang++
CXXFLAGS := -std=c++20
CPPFLAGS := -MMD -MP

TARGETS := test
UNITS_test := test archive

include Makefile.template
