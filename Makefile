CC = cl
CXX = cl
LINK = link
RC = rc
CFLAGS = /nologo /W3 /Gy /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /I. /FD /c
CXXFLAGS = $(CFLAGS)
CPPFLAGS =
RCFLAGS =
LDFLAGS = /nologo /subsystem:windows
LIBS = kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib scrnsave.lib comctl32.lib
OUT = Matrix32.scr

.c.obj:
	$(CC) $(CFLAGS) $(CPPFLAGS) /c $<
	
.cpp.obj:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) /c $<
	
.rc.res:
	$(RC) $(RCFLAGS) $<

obj_files = Matrix.obj \
	Matrix.res

all: $(obj_files)
	$(LINK) @<<
		$(LDFLAGS) /OUT:$(OUT) $(LIBS) $(obj_files)
<<

clean:
	del $(obj_files) $(OUT) *.obj *.res