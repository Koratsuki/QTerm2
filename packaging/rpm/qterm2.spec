Name:           qterm2
Version:        1.0.0
Release:        1%{?dist}
Summary:        Terminal emulator inspired by iTerm2

License:        MIT
URL:            https://github.com/yourusername/QTerm2
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  libxkbcommon-devel
BuildRequires:  openssl-devel

Requires:       qt6-qtbase
Requires:       openssh-clients

%description
QTerm2 is a terminal emulator for Linux inspired by iTerm2, written in C++17 with Qt 6 Widgets.
Features include SSH session manager, tabs, splits, xterm-256color emulator, plugins, and more.

%prep
%autosetup

%build
%cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/scalable/apps
install -m 644 packaging/QTerm2.desktop %{buildroot}%{_datadir}/applications/
install -m 644 packaging/QTerm2.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%files
%license LICENSE
%doc README.md
%{_bindir}/QTerm2
%{_datadir}/applications/QTerm2.desktop
%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%changelog
* Fri Oct 25 2026 Your Name <your.email@example.com> - 1.0.0-1
- Initial release
