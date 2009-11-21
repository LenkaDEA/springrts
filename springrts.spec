%define _unpackaged_files_terminate_build 1
%def_disable debug

Name: springrts
Version: 0.80.5.1
Release: alt1

Summary: Real time strategy game engine with many mods
License: GPL2+ or Artistic
Group: Games/Strategy 
Url: http://springrts.com/

Packager: Maxim Ivanov <redbaron@altlinux.org>

BuildRequires(pre): rpm-build-xdg
#BuildRequires: boost-program_options-devel cmake gcc-c++ libSDL-devel 

BuildRequires: cmake java-devel libGL-devel libGLU-devel gcc-c++
BuildRequires: boost-devel boost-program_options-devel boost-asio-devel boost-signals-devel
BuildRequires: libICE-devel libSM-devel libX11-devel libXdamage-devel libXfixes-devel libXrender-devel 
BuildRequires: libXt-devel libfreetype-devel libogg-devel libstdc++-devel 
BuildRequires: xorg-inputproto-devel xorg-kbproto-devel xorg-xextproto-devel xorg-xf86miscproto-devel 
BuildRequires: xorg-xineramaproto-devel xorg-xproto-devel zlib-devel
BuildRequires: libdevil-devel libfreeglut-devel libglew-devel libopenal1-devel 
BuildRequires: libvorbis-devel  python-devel libSDL-devel

#BuildRequires: boost-asio-devel boost-program_options-devel boost-signals-devel ccmake gcc-c++ java-devel  libXScrnSaver-devel libXau-devel libXcomposite-devel libXcursor-devel libXdmcp-devel libXext-devel libXft-devel libXi-devel libXinerama-devel libXmu-devel libXpm-devel libXrandr-devel libXtst-devel libXv-devel libXxf86misc-devel libdevil-devel libfreeglut-devel libglew-devel libopenal1-devel libvorbis-devel libxkbfile-devel tzdata xorg-xf86vidmodeproto-devel zip


Requires: %name-data = %version-%release
Source0: %name-%version.tar

%description
Spring is an open source RTS (Real time Strategy) engine originally
designed to recreate the experience of Total Annihilation.  Spring now
supports many different games ("mods"), including both remakes of the
original Total Annihilation and completely new games.

This package contains the game engine and default AI, but no maps, mods,
or user interface.

%package data
Summary: data files for Spring RTS engine
Group: Games/Strategy
BuildArch: noarch

%description data
data files for Spring RTS engine

%package dedicated
Summary: springrts dedicated server
Group: Games/Strategy
Requires: %name-data = %version-%release

%description dedicated
springrts dedicated server

%prep
%setup 

%build
mkdir build
pushd build
cmake .. \
%if_enabled debug
        -DCMAKE_BUILD_TYPE=Debug \
%else
        -DCMAKE_BUILD_TYPE=Release \
%endif
        -DCMAKE_INSTALL_PREFIX=%_prefix \
        -DBINDIR=%_gamesbindir \
        -DAI_LIBS_DIR=%_libdir/spring
%make_build

%install
cd build
%make install DESTDIR=%buildroot
mkdir %buildroot%_gamesdatadir/spring/{mods,maps}

%if_enabled debug
%add_strip_skiplist %_bindir/*
%add_strip_skiplist %_libdir/*
%endif

%files 
%_gamesbindir/spring
%_libdir/spring

%files data
%_gamesdatadir/*
%_pixmapsdir/*
%_xdgmimedir/packages/*
%_desktopdir/*

%files dedicated
%_gamesbindir/spring-dedicated
%_libdir/*.so

%post data
  [ -f %_gamesdatadir/spring/base/otacontent.sdz ] && \
  [ -f %_gamesdatadir/spring/base/tacontent_v2.sdz ] && \
  [ -f %_gamesdatadir/spring/base/tatextures_v062.sdz ] && exit 0

  echo " ================= Non-free content not included  ==================="
  echo "  Please download and install additional non-free content which      "
  echo "  could not be included in this package.                             "
  echo ""
  echo "   1. download http://files.simhost.org/Spring/base-ota-content.zip  "
  echo "   2. extract it to %_gamesdatadir/spring/base                       "
  echo " ===================================================================="

%changelog
* Sat Oct 31 2009 Maxim Ivanov <redbaron at altlinux.org> 0.80.5.1-alt1
- Updated to 0.80.5.1
- Dedicated server packed separately

* Sat Sep 05 2009 Maxim Ivanov <redbaron at altlinux.org> 0.80.4.1-alt1
- Update to 0.80.4.1

* Sat Aug 29 2009 Maxim Ivanov <redbaron at altlinux.org> 0.80.2-alt1
- Update to 0.80.2

* Sun Jun 07 2009 Maxim Ivanov <redbaron at altlinux.org> 0.79.1-alt1
- Initial build for ALTLinux
