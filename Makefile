
CC = cl.exe
LINK = link.exe
DIR = bin\\
NAME = socks
OUTNAME = $(DIR)$(NAME).exe

CFLAGS = /nologo /D_DBG /MT /c /EHs /Zi
LFLAGS = /DEBUG /SUBSYSTEM:WINDOWS /ENTRY:main /OUT:$(OUTNAME)

clean:
	@del $(NAME).obj \
		$(DIR)$(NAME).ilk \
		$(DIR)$(NAME).pdb \
		$(OUTNAME)

SOURCES = main.cpp Socks.cpp Thread.cpp functions.cpp
OBJS = main.obj Socks.obj Thread.obj functions.obj
LIBS = kernel32.lib user32.lib shlwapi.lib ws2_32.lib msvcrt.lib

build: 
	$(CC) $(CFLAGS) $(SOURCES)
	$(LINK) $(LFLAGS) $(OBJS) $(LIBS)
