FLAGS:= -I ./TX -I ./include -Wshadow -Winit-self -Wredundant-decls -Wcast-align -Wundef -Wfloat-equal -Winline -Wunreachable-code -Wmissing-declarations -Wmissing-include-dirs -Wswitch-enum -Wswitch-default -Weffc++ -Wmain -Wextra -Wall -g -pipe -fexceptions -Wcast-qual -Wconversion -Wctor-dtor-privacy -Wempty-body -Wformat-security -Wformat=2 -Wignored-qualifiers -Wlogical-op -Wno-missing-field-initializers -Wnon-virtual-dtor -Woverloaded-virtual -Wpointer-arith -Wsign-promo -Wstack-usage=8192 -Wstrict-aliasing -Wstrict-null-sentinel -Wtype-limits -Wwrite-strings -Werror=vla -D_DEBUG -D_EJUDGE_CLIENT_SIDE
BINDIR:=bin
OUTPUT:=akin.exe
SRCDIR:=source
SOURCE:=$(wildcard ${SRCDIR}/*.cpp)
OBJECTS:=$(addsuffix .o,$(addprefix ${BINDIR}\,$(basename $(notdir ${SOURCE}))))
LOGS:=logs

all: ${OUTPUT}

${OUTPUT}:${OBJECTS} ${LOGS}
	g++ ${FLAGS} ${OBJECTS} -o ${OUTPUT} -lsapi -lole32
${OBJECTS}: ${SOURCE} ${BINDIR}
	$(foreach SRC,${SOURCE},$(shell g++ -c ${SRC} ${FLAGS} -o $(addsuffix .o,$(addprefix ${BINDIR}\,$(basename $(notdir ${SRC}))))  -lsapi -lole32))
clean:
	$(foreach OBJ,${OBJECTS}, $(shell del ${OBJ}))
	del ${OUTPUT}
	rd ${BINDIR}
${SOURCE}:

${BINDIR}:
	md ${BINDIR}
${LOGS}:
	md ${LOGS}
	md ${LOGS}\img
	md ${LOGS}\dot_cp1251
	md ${LOGS}\dot_utf8
