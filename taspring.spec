%define _unpackaged_files_terminate_build 1

Name: springrts
Version: 0.80.2
Release: alt1

Summary: Total Ahnigilation rewrite
License: GPL or Artistic
Group: Games/Strategy 
Url: http://springrts.com/

Packager: Maxim Ivanov <redbaron@altlinux.org>

BuildRequires: boost-program_options-devel cmake gcc-c++ libSDL-devel 
BuildRequires: libdevil-devel libfreeglut-devel libglew-devel libopenal1-devel 
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
* Sat Aug 29 2009 Maxim Ivanov <redbaron at altlinux.org> 0.80.2-alt1
- Update to 0.80.2

* Sun Jun 07 2009 Maxim Ivanov <redbaron at altlinux.org> 0.79.1-alt1
- Initial build for ALTLinux 



