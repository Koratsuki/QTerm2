Name:           qterm2
Version:        1.0.6
Release:        1%{?dist}
Summary:        Terminal emulator inspired by iTerm2

License:        MIT
URL:            https://github.com/Koratsuki/QTerm2
Source0:        qterm2-1.0.6.tar.gz

%description
QTerm2 is a terminal emulator for Linux inspired by iTerm2, written in C++17 with Qt 6 Widgets.

%prep
%autosetup

%build
# Build already performed in CI, just install
%install
mkdir -p %{buildroot}%{_bindir}
install -m 755 build/QTerm2 %{buildroot}%{_bindir}/QTerm2
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/scalable/apps
install -m 644 packaging/QTerm2.desktop %{buildroot}%{_datadir}/applications/
install -m 644 packaging/QTerm2.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%files
%{_bindir}/QTerm2
%{_datadir}/applications/QTerm2.desktop
%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%changelog
* Mon Oct 26 2026 Leslie León <koratsuki.nijuusan@gmail.com> - 1.0.6-1
- Release 1.0.6
