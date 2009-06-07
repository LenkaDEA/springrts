%define _unpackaged_files_terminate_build 1

Name: taspring
Version: 0.79.1
Release: alt1

Summary: Total Ahnigilation rewrite
License: GPL or Artistic
Group: Games/Strategy 
Url: http://springrts.com/

Packager: Maxim Ivanov <redbaron@altlinux.org>

BuildRequires: boost-program_options-devel cmake gcc-c++ libSDL-devel 
#BuildRequires: libXScrnSaver-devel libXau-devel libXcomposite-devel libXcursor-devel libXdmcp-devel libXext-devel libXft-devel libXi-devel libXinerama-devel libXmu-devel libXpm-devel libXrandr-devel libXtst-devel libXv-devel libXxf86misc-devel xorg-xf86vidmodeproto-devel zip libxkbfile-devel
BuildRequires: libdevil-devel libfreeglut-devel libglew-devel libopenal-devel 
BuildRequires: libvorbis-devel  python-devel  

Source0: %name-%version.tar

%description
TA:3D descr

%prep
%setup 

%build
mkdir build
pushd build
cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=%_prefix \
        -DBINDIR=%_gamesbindir
%make_build

%install
cd build
%make install DESTDIR=%buildroot

%files 
%_gamesbindir/*
%_libdir/*
%_gamesdatadir/*
%_desktopdir/*
%_K4xdg_mime/*
%_pixmapsdir/*

%changelog
* Sun Jun 07 2009 Maxim Ivanov <redbaron at altlinux.org> 0.79.1-alt1
- Initial build for ALTLinux 



