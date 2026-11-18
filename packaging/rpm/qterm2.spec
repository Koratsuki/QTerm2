Name:           qterm2
Version:        1.1.1
Release:        1%{?dist}
Summary:        Terminal emulator inspired by iTerm2

License:        MIT
URL:            https://github.com/Koratsuki/QTerm2
Source0:        qterm2-%{version}.tar.gz
# The CI builds the binary before packaging; git archive only carries tracked
# sources, so the compiled executable is supplied separately as Source1.
Source1:        QTerm2
Requires:       qt6-qtbase >= 6.2
Requires:       openssh-clients

%description
QTerm2 is a terminal emulator for Linux inspired by iTerm2, written in C++17 with Qt 6 Widgets.

%prep
%autosetup -c

%build
# Build already performed in CI, just install

%install
mkdir -p %{buildroot}%{_bindir}
install -m 755 %{SOURCE1} %{buildroot}%{_bindir}/QTerm2
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/scalable/apps
install -m 644 packaging/QTerm2.desktop %{buildroot}%{_datadir}/applications/
install -m 644 packaging/QTerm2.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%files
%{_bindir}/QTerm2
%{_datadir}/applications/QTerm2.desktop
%{_datadir}/icons/hicolor/scalable/apps/QTerm2.svg

%changelog
* Tue Nov 17 2026 Leslie León <koratsuki.nijuusan@gmail.com> - 1.1.1-1
- SFTP browser with async transfer queue and remote editor
- SSH tunnel manager with persistence, logs, and reconnect
- Network discovery and session creation from discovered hosts
- Broadcast input, tab UX improvements, and command palette shortcut
- Plugin permission confirmation and integrity/signature verification
- Security, i18n, refactors, and stability fixes

* Mon Oct 26 2026 Leslie León <koratsuki.nijuusan@gmail.com> - 1.1.0-1
- Release 1.1.0
