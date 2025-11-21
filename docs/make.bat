@ECHO OFF

set SPHINXBUILD=sphinx-build
set SOURCEDIR=.
set BUILDDIR=_build
set SPHINXOPTS=

if "%1"=="" goto help

%SPHINXBUILD% -M %1 %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%
goto end

:help
%SPHINXBUILD% -M help %SOURCEDIR% %BUILDDIR% %SPHINXOPTS%

:end
